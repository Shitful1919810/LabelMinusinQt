#include "ui/ImageCanvas.h"

#include <QApplication>
#include <QBrush>
#include <QGraphicsPixmapItem>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
constexpr int markerType = QGraphicsItem::UserType + 100;

class LabelMarkerItem final : public QGraphicsItem {
public:
    LabelMarkerItem(int labelIndex, bool selected, labelminus::core::LabelGroupStyle style,
                    QGraphicsItem* parent = nullptr)
        : QGraphicsItem(parent), m_labelIndex(labelIndex), m_selected(selected), m_style(std::move(style))
    {
        setFlag(QGraphicsItem::ItemIgnoresTransformations);
        setZValue(10.0);
    }

    int type() const override
    {
        return markerType;
    }

    int labelIndex() const noexcept
    {
        return m_labelIndex;
    }

    QRectF boundingRect() const override
    {
        const double radius = m_style.markerDiameter / 2.0;
        return QRectF(-radius, -radius, m_style.markerDiameter, m_style.markerDiameter);
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(QPen(m_selected ? QColor(46, 103, 230) : Qt::white, m_selected ? 3.0 : 1.5));
        painter->setBrush(m_style.groupColor.isValid() ? m_style.groupColor : Qt::black);
        const QRectF shapeRect = boundingRect().adjusted(1.0, 1.0, -1.0, -1.0);
        if (m_style.markerShape == labelminus::core::MarkerShape::Square) {
            painter->drawRect(shapeRect);
        }
        else {
            painter->drawEllipse(shapeRect);
        }

        painter->setPen(Qt::white);
        QFont font = painter->font();
        font.setPointSizeF(m_style.fontPointSize);
        font.setBold(true);
        painter->setFont(font);
        const QString number = QString::number(m_labelIndex + 1);
        painter->drawText(boundingRect(), Qt::AlignCenter, number);
    }

private:
    int m_labelIndex;
    bool m_selected;
    labelminus::core::LabelGroupStyle m_style;
};
} // namespace

ImageCanvas::ImageCanvas(QWidget* parent) : QGraphicsView(parent)
{
    setScene(&m_scene);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);

    m_hoverToolTip = new QLabel(this, Qt::ToolTip);
    m_hoverToolTip->setAttribute(Qt::WA_ShowWithoutActivating);
    m_hoverToolTip->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_hoverToolTip->setTextFormat(Qt::RichText);
    m_hoverToolTip->setMargin(6);
    m_hoverToolTip->setStyleSheet(QStringLiteral("QLabel {"
                                                 "background: palette(toolTipBase);"
                                                 "color: palette(toolTipText);"
                                                 "border: 1px solid palette(mid);"
                                                 "border-radius: 3px;"
                                                 "}"));
    m_hoverToolTip->hide();
}

void ImageCanvas::setPreferences(const labelminus::core::AppPreferences& preferences)
{
    m_markerDiameterPixels = preferences.labelMarkerDiameterPixels();
    m_markerFontPointSize = preferences.labelMarkerFontPointSize();
    m_moveLabelModifiers = preferences.moveLabelModifiers();
    m_groupStyles = preferences.groupStyles();
    rebuildLabelItems();
}

void ImageCanvas::setImage(const QString& path, const QVector<labelminus::core::Label>& labels)
{
    m_imagePath = path;
    m_labels = labels;
    m_selectedLabel = -1;

    QPixmap pixmap(path);
    m_scene.clear();
    m_labelItems.clear();
    m_pixmapItem = m_scene.addPixmap(pixmap);
    m_scene.setSceneRect(m_pixmapItem->boundingRect());
    rebuildLabelItems();

    if (!m_hasUserZoom) {
        fitInView(m_pixmapItem->boundingRect(), Qt::KeepAspectRatio);
        updateScenePadding();
    }
    else {
        applyZoom();
    }
}

void ImageCanvas::setLabels(const QVector<labelminus::core::Label>& labels)
{
    m_labels = labels;
    if (m_selectedLabel >= m_labels.size()) {
        m_selectedLabel = -1;
    }
    rebuildLabelItems();
}

void ImageCanvas::setGroups(QStringList groups)
{
    m_groups = std::move(groups);
    rebuildLabelItems();
}

void ImageCanvas::setVisibleGroups(QStringList groups)
{
    m_visibleGroups = QSet<QString>(groups.cbegin(), groups.cend());
    hideHoveredLabelToolTip();
    rebuildLabelItems();
}

void ImageCanvas::setSelectedLabel(int index)
{
    if (m_selectedLabel == index) {
        return;
    }

    m_selectedLabel = index;
    rebuildLabelItems();
}

void ImageCanvas::setZoomPercent(int percent)
{
    m_hasUserZoom = true;
    m_zoomPercent = std::clamp(percent, 10, 400);
    applyZoom();
}

int ImageCanvas::zoomPercent() const noexcept
{
    return m_zoomPercent;
}

QPointF ImageCanvas::normalizedViewCenter() const
{
    if (m_pixmapItem == nullptr) {
        return QPointF(0.5, 0.5);
    }

    return normalizedPositionFromScene(mapToScene(viewport()->rect().center()));
}

void ImageCanvas::restoreView(int zoomPercent, QPointF normalizedCenter)
{
    if (m_pixmapItem == nullptr) {
        return;
    }

    m_hasUserZoom = true;
    m_zoomPercent = std::clamp(zoomPercent, 10, 400);
    applyZoom();

    normalizedCenter.setX(std::clamp(normalizedCenter.x(), 0.0, 1.0));
    normalizedCenter.setY(std::clamp(normalizedCenter.y(), 0.0, 1.0));
    const QRectF rect = m_pixmapItem->boundingRect();
    centerOn(rect.left() + normalizedCenter.x() * rect.width(), rect.top() + normalizedCenter.y() * rect.height());
}

void ImageCanvas::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        setFocus();
        m_pendingLabelCreate = false;

        QGraphicsItem* item = itemAt(event->pos());
        while (item != nullptr) {
            if (item->type() == markerType) {
                auto* marker = static_cast<LabelMarkerItem*>(item);
                if (hasMoveLabelModifiers(event->modifiers())) {
                    m_isMovingLabel = true;
                    m_movingLabelIndex = marker->labelIndex();
                    m_pendingLabelCreate = false;
                    emit labelSelected(m_movingLabelIndex);
                    hideHoveredLabelToolTip();
                    event->accept();
                    return;
                }
                emit labelSelected(marker->labelIndex());
                event->accept();
                return;
            }
            item = item->parentItem();
        }

        if (m_pixmapItem != nullptr) {
            const QPointF scenePosition = mapToScene(event->pos());
            if (m_pixmapItem->contains(scenePosition)) {
                m_pendingLabelCreate = true;
                m_labelCreatePressPosition = event->pos();
            }
        }
    }

    QGraphicsView::mousePressEvent(event);
}

void ImageCanvas::mouseMoveEvent(QMouseEvent* event)
{
    if (m_isMovingLabel && m_pixmapItem != nullptr && m_movingLabelIndex >= 0 && m_movingLabelIndex < m_labels.size()) {
        m_labels[m_movingLabelIndex].setPosition(normalizedPositionFromScene(mapToScene(event->pos())));
        rebuildLabelItems();
        event->accept();
        return;
    }

    if (m_pendingLabelCreate &&
        (event->pos() - m_labelCreatePressPosition).manhattanLength() >= QApplication::startDragDistance()) {
        m_pendingLabelCreate = false;
    }

    QGraphicsView::mouseMoveEvent(event);
    if (event->buttons() == Qt::NoButton) {
        updateHoveredLabelToolTip(event->pos(), event->globalPosition().toPoint());
    }
    else {
        hideHoveredLabelToolTip();
    }
}

void ImageCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_isMovingLabel) {
        const int labelIndex = m_movingLabelIndex;
        m_isMovingLabel = false;
        m_movingLabelIndex = -1;
        if (m_pixmapItem != nullptr && labelIndex >= 0 && labelIndex < m_labels.size()) {
            const QPointF normalizedPosition = normalizedPositionFromScene(mapToScene(event->pos()));
            m_labels[labelIndex].setPosition(normalizedPosition);
            rebuildLabelItems();
            emit labelMoveRequested(labelIndex, m_labels.at(labelIndex).position());
        }
        event->accept();
        return;
    }

    const bool shouldCreateLabel =
        event->button() == Qt::LeftButton && m_pendingLabelCreate &&
        (event->pos() - m_labelCreatePressPosition).manhattanLength() < QApplication::startDragDistance() &&
        m_pixmapItem != nullptr && m_pixmapItem->contains(mapToScene(event->pos()));

    m_pendingLabelCreate = false;
    QGraphicsView::mouseReleaseEvent(event);

    if (shouldCreateLabel) {
        emit labelCreateRequested(normalizedPositionFromScene(mapToScene(event->pos())));
    }
}

void ImageCanvas::wheelEvent(QWheelEvent* event)
{
    const int step = event->angleDelta().y() > 0 ? 10 : -10;
    setZoomPercentAt(m_zoomPercent + step, event->position().toPoint());
    emit zoomPercentChanged(m_zoomPercent);
    event->accept();
}

void ImageCanvas::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    if (!m_hasUserZoom && m_pixmapItem != nullptr) {
        fitInView(m_pixmapItem->boundingRect(), Qt::KeepAspectRatio);
    }
    updateScenePadding();
}

void ImageCanvas::leaveEvent(QEvent* event)
{
    hideHoveredLabelToolTip();
    QGraphicsView::leaveEvent(event);
}

void ImageCanvas::rebuildLabelItems()
{
    for (QGraphicsItem* item : m_labelItems) {
        m_scene.removeItem(item);
        delete item;
    }
    m_labelItems.clear();

    if (m_pixmapItem == nullptr) {
        return;
    }

    const QRectF rect = m_pixmapItem->boundingRect();
    for (int i = 0; i < m_labels.size(); ++i) {
        if (!isLabelVisible(m_labels.at(i))) {
            continue;
        }

        auto* marker = new LabelMarkerItem(i, i == m_selectedLabel, styleForGroup(m_labels.at(i).group()));
        const QPointF position = m_labels.at(i).position();
        marker->setPos(rect.left() + position.x() * rect.width(), rect.top() + position.y() * rect.height());
        m_scene.addItem(marker);
        m_labelItems.append(marker);
    }
}

void ImageCanvas::applyZoom()
{
    resetTransform();
    const double scaleFactor = static_cast<double>(m_zoomPercent) / 100.0;
    scale(scaleFactor, scaleFactor);
    updateScenePadding();
}

void ImageCanvas::updateScenePadding()
{
    if (m_pixmapItem == nullptr) {
        return;
    }

    const QRectF imageRect = m_pixmapItem->boundingRect();
    const double scaleFactor = std::max(std::abs(transform().m11()), 0.001);
    const double horizontalPadding = static_cast<double>(viewport()->width()) / scaleFactor;
    const double verticalPadding = static_cast<double>(viewport()->height()) / scaleFactor;
    m_scene.setSceneRect(imageRect.adjusted(-horizontalPadding, -verticalPadding, horizontalPadding, verticalPadding));
}

void ImageCanvas::setZoomPercentAt(int percent, QPoint viewportAnchor)
{
    if (m_pixmapItem == nullptr) {
        setZoomPercent(percent);
        return;
    }

    m_hasUserZoom = true;
    const QPointF sceneAnchor = mapToScene(viewportAnchor);
    m_zoomPercent = std::clamp(percent, 10, 400);
    applyZoom();

    const QPoint viewportAnchorAfter = mapFromScene(sceneAnchor);
    const QPoint delta = viewportAnchorAfter - viewportAnchor;
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() + delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() + delta.y());
}

bool ImageCanvas::isLabelVisible(const labelminus::core::Label& label) const
{
    return !label.isDeleted() && m_visibleGroups.contains(label.group());
}

bool ImageCanvas::hasMoveLabelModifiers(Qt::KeyboardModifiers modifiers) const
{
    constexpr Qt::KeyboardModifiers relevantModifiers =
        Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier;
    return (modifiers & relevantModifiers) == m_moveLabelModifiers;
}

void ImageCanvas::updateHoveredLabelToolTip(const QPoint& viewportPosition, const QPoint& globalPosition)
{
    QStringList lines;
    QSet<int> seenLabels;
    const QList<QGraphicsItem*> hoveredItems = items(viewportPosition);
    for (QGraphicsItem* item : hoveredItems) {
        while (item != nullptr && item->type() != markerType) {
            item = item->parentItem();
        }
        if (item == nullptr) {
            continue;
        }

        const auto* marker = static_cast<LabelMarkerItem*>(item);
        const int labelIndex = marker->labelIndex();
        if (seenLabels.contains(labelIndex) || labelIndex < 0 || labelIndex >= m_labels.size()) {
            continue;
        }

        seenLabels.insert(labelIndex);
        const labelminus::core::Label& label = m_labels.at(labelIndex);
        if (!isLabelVisible(label)) {
            continue;
        }

        const QColor color = styleForGroup(label.group()).groupColor.isValid() ? styleForGroup(label.group()).groupColor
                                                                               : QColor(Qt::black);
        lines.append(QStringLiteral("<span style=\"color:%1; font-weight:600;\">#%2</span> : %3")
                         .arg(color.name(), QString::number(labelIndex + 1), label.text().toHtmlEscaped()));
    }

    if (lines.isEmpty()) {
        hideHoveredLabelToolTip();
        return;
    }

    m_hoverToolTip->setText(lines.join(QStringLiteral("<br/>")));
    m_hoverToolTip->adjustSize();
    m_hoverToolTip->move(globalPosition + QPoint(12, 18));
    m_hoverToolTip->show();
    m_hoverToolTip->raise();
}

void ImageCanvas::hideHoveredLabelToolTip()
{
    if (m_hoverToolTip != nullptr) {
        m_hoverToolTip->hide();
    }
}

labelminus::core::LabelGroupStyle ImageCanvas::styleForGroup(const QString& group) const
{
    const int index = static_cast<int>(m_groups.indexOf(group));
    if (index < 0 || index >= static_cast<int>(m_groupStyles.size())) {
        return {QColor(), m_markerDiameterPixels, m_markerFontPointSize, labelminus::core::MarkerShape::Circle};
    }
    return m_groupStyles.at(index);
}

QPointF ImageCanvas::normalizedPositionFromScene(QPointF scenePosition) const
{
    if (m_pixmapItem == nullptr) {
        return QPointF(0.0, 0.0);
    }

    const QRectF rect = m_pixmapItem->boundingRect();
    const double x = (scenePosition.x() - rect.left()) / rect.width();
    const double y = (scenePosition.y() - rect.top()) / rect.height();
    return QPointF(std::clamp(x, 0.0, 1.0), std::clamp(y, 0.0, 1.0));
}
