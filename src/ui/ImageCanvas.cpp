#include "ui/ImageCanvas.h"

#include <QApplication>
#include <QBrush>
#include <QGraphicsPixmapItem>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QWheelEvent>

#include <algorithm>
#include <utility>

namespace {
constexpr int markerType = QGraphicsItem::UserType + 100;

class LabelMarkerItem final : public QGraphicsItem {
public:
    LabelMarkerItem(int labelIndex, bool selected, double diameter, double fontPointSize, QColor color,
                    QGraphicsItem* parent = nullptr)
        : QGraphicsItem(parent), m_labelIndex(labelIndex), m_selected(selected), m_diameter(diameter),
          m_fontPointSize(fontPointSize), m_color(std::move(color))
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
        const double radius = m_diameter / 2.0;
        return QRectF(-radius, -radius, m_diameter, m_diameter);
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(QPen(m_selected ? QColor(46, 103, 230) : Qt::white, m_selected ? 3.0 : 1.5));
        painter->setBrush(m_color.isValid() ? m_color : Qt::black);
        painter->drawEllipse(boundingRect().adjusted(1.0, 1.0, -1.0, -1.0));

        painter->setPen(Qt::white);
        QFont font = painter->font();
        font.setPointSizeF(m_fontPointSize);
        font.setBold(true);
        painter->setFont(font);
        const QString number = QString::number(m_labelIndex + 1);
        painter->drawText(boundingRect(), Qt::AlignCenter, number);
    }

private:
    int m_labelIndex;
    bool m_selected;
    double m_diameter;
    double m_fontPointSize;
    QColor m_color;
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
}

void ImageCanvas::setPreferences(const labelminus::core::AppPreferences& preferences)
{
    m_markerDiameterPixels = preferences.labelMarkerDiameterPixels();
    m_markerFontPointSize = preferences.labelMarkerFontPointSize();
    m_groupColors = preferences.groupColors();
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
        fitInView(m_scene.sceneRect(), Qt::KeepAspectRatio);
    }
    else {
        applyZoom();
    }
}

void ImageCanvas::setGroups(QStringList groups)
{
    m_groups = std::move(groups);
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

void ImageCanvas::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::Undo)) {
        emit undoRequested();
        event->accept();
        return;
    }

    QGraphicsView::keyPressEvent(event);
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
    if (m_pendingLabelCreate &&
        (event->pos() - m_labelCreatePressPosition).manhattanLength() >= QApplication::startDragDistance()) {
        m_pendingLabelCreate = false;
    }

    QGraphicsView::mouseMoveEvent(event);
}

void ImageCanvas::mouseReleaseEvent(QMouseEvent* event)
{
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
    setZoomPercent(m_zoomPercent + step);
    emit zoomPercentChanged(m_zoomPercent);
    event->accept();
}

void ImageCanvas::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    if (!m_hasUserZoom && m_pixmapItem != nullptr) {
        fitInView(m_scene.sceneRect(), Qt::KeepAspectRatio);
    }
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
    const double markerDiameter = markerDiameterForCurrentImage();
    const double markerFontPointSize = markerFontPointSizeForCurrentImage();
    for (int i = 0; i < m_labels.size(); ++i) {
        if (m_labels.at(i).isDeleted()) {
            continue;
        }

        auto* marker = new LabelMarkerItem(i, i == m_selectedLabel, markerDiameter, markerFontPointSize,
                                           colorForGroup(m_labels.at(i).group()));
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
}

double ImageCanvas::markerDiameterForCurrentImage() const
{
    return m_markerDiameterPixels;
}

double ImageCanvas::markerFontPointSizeForCurrentImage() const
{
    return m_markerFontPointSize;
}

QColor ImageCanvas::colorForGroup(const QString& group) const
{
    const int index = static_cast<int>(m_groups.indexOf(group));
    if (index < 0 || index >= static_cast<int>(m_groupColors.size())) {
        return {};
    }
    return m_groupColors.at(index);
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
