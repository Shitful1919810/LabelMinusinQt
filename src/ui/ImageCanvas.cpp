#include "ui/ImageCanvas.h"

#include <QApplication>
#include <QBrush>
#include <QClipboard>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPixmap>
#include <QScrollBar>
#include <QTextDocument>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
constexpr int markerType = QGraphicsItem::UserType + 100;

QString htmlEscapedWithLineBreaks(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    QStringList escapedLines;
    const QStringList lines = text.split(QLatin1Char('\n'));
    escapedLines.reserve(lines.size());
    for (const QString& line : lines) {
        escapedLines.append(line.toHtmlEscaped());
    }
    return escapedLines.join(QStringLiteral("<br/>"));
}

QString labelBubbleHtml(int labelIndex, const QString& text, const QColor& color)
{
    return QStringLiteral("<span style=\"color:%1; font-weight:600;\">#%2</span> : %3")
        .arg(color.name(), QString::number(labelIndex + 1), htmlEscapedWithLineBreaks(text));
}

class LabelMarkerItem final : public QGraphicsItem {
public:
    LabelMarkerItem(int labelIndex, bool selected, labelqt::core::LabelGroupStyle style,
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
        if (m_style.markerShape == labelqt::core::MarkerShape::Square) {
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
    labelqt::core::LabelGroupStyle m_style;
};

class LabelTextBubbleItem final : public QGraphicsItem {
public:
    LabelTextBubbleItem(int labelIndex, QString text, labelqt::core::LabelGroupStyle style, QFont bubbleFont,
                        double opacity, QGraphicsItem* parent = nullptr)
        : QGraphicsItem(parent), m_labelIndex(labelIndex), m_text(std::move(text)), m_style(std::move(style)),
          m_bubbleFont(std::move(bubbleFont)), m_opacity(opacity)
    {
        setFlag(QGraphicsItem::ItemIgnoresTransformations);
        setZValue(11.0);
        rebuildDocument();
    }

    QRectF boundingRect() const override
    {
        const QSizeF textSize = m_document.size();
        return QRectF(m_style.markerDiameter / 2.0 + 8.0, -textSize.height() / 2.0 - 6.0, textSize.width() + 16.0,
                      textSize.height() + 12.0);
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setOpacity(m_opacity);
        const QPalette palette = QApplication::palette();
        painter->setPen(QPen(palette.color(QPalette::Mid), 1.0));
        painter->setBrush(palette.color(QPalette::ToolTipBase));
        painter->drawRoundedRect(boundingRect(), 3.0, 3.0);

        painter->save();
        painter->translate(boundingRect().topLeft() + QPointF(8.0, 6.0));
        m_document.drawContents(painter);
        painter->restore();
    }

private:
    void rebuildDocument()
    {
        const QColor color = m_style.groupColor.isValid() ? m_style.groupColor : QColor(Qt::black);
        m_document.setDefaultFont(m_bubbleFont);
        m_document.setDocumentMargin(0.0);
        m_document.setHtml(labelBubbleHtml(m_labelIndex, m_text, color));
    }

    int m_labelIndex;
    QString m_text;
    labelqt::core::LabelGroupStyle m_style;
    QFont m_bubbleFont;
    double m_opacity{1.0};
    QTextDocument m_document;
};
} // namespace

ImageCanvas::ImageCanvas(QWidget* parent) : QGraphicsView(parent)
{
    m_textBubbleFont = QApplication::font();

    setScene(&m_scene);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    updateCursorForInteractionMode();
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, &ImageCanvas::notifyViewportStateChanged);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &ImageCanvas::notifyViewportStateChanged);

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

ImageCanvas::~ImageCanvas()
{
    m_isDestroying = true;
    disconnect(horizontalScrollBar(), nullptr, this, nullptr);
    disconnect(verticalScrollBar(), nullptr, this, nullptr);
    hideHoveredLabelToolTip();
    clearSceneItems();
    setScene(nullptr);
}

void ImageCanvas::setInteractionMode(InteractionMode mode)
{
    if (m_interactionMode == mode) {
        return;
    }

    m_interactionMode = mode;
    m_pendingLabelCreate = false;
    m_pendingLabelSelect = false;
    m_pendingLabelSelectIndex = -1;
    m_pendingLabelSelectModifiers = Qt::NoModifier;
    m_pendingLabelMove = false;
    m_pendingLabelMoveIndex = -1;
    m_isMovingLabel = false;
    m_movingLabelIndex = -1;
    m_isSelectingRegion = false;
    m_isMiddleButtonPanning = false;
    hideHoveredLabelToolTip();
    setDragMode(m_interactionMode == InteractionMode::Label ? QGraphicsView::ScrollHandDrag : QGraphicsView::NoDrag);
    updateCursorForInteractionMode();
    if (m_interactionMode == InteractionMode::Label) {
        clearSelection();
        emit selectionChanged({});
    }
    rebuildLabelItems();
}

ImageCanvas::InteractionMode ImageCanvas::interactionMode() const noexcept
{
    return m_interactionMode;
}

void ImageCanvas::setPreferences(const labelqt::core::AppPreferences& preferences)
{
    m_markerDiameterPixels = preferences.labelMarkerDiameterPixels();
    m_markerFontPointSize = preferences.labelMarkerFontPointSize();
    m_textBubbleOpacity = preferences.markerTextBubbleOpacity();
    m_hoverToolTip->setWindowOpacity(m_textBubbleOpacity);
    m_textBubbleFont = QApplication::font();
    if (!preferences.markerTextBubbleFontFamily().isEmpty()) {
        m_textBubbleFont.setFamily(preferences.markerTextBubbleFontFamily());
    }
    if (preferences.markerTextBubbleFontPointSize() > 0.0) {
        m_textBubbleFont.setPointSizeF(preferences.markerTextBubbleFontPointSize());
    }
    m_moveLabelModifiers = preferences.moveLabelModifiers();
    m_groupStyles = preferences.groupStyles();
    rebuildLabelItems();
}

void ImageCanvas::setImage(const QString& path, const QVector<labelqt::core::Label>& labels)
{
    hideHoveredLabelToolTip();
    m_imagePath = path;
    m_labels = labels;
    m_selectedLabels.clear();
    m_labelTextPreviews.clear();
    m_normalizedSelectionRect = {};
    m_isSelectingRegion = false;

    QPixmap pixmap(path);
    clearSceneItems();
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

void ImageCanvas::clearSceneItems()
{
    m_pixmapItem = nullptr;
    m_selectionItem = nullptr;
    m_labelItems.clear();
    m_scene.clear();
}

void ImageCanvas::setLabels(const QVector<labelqt::core::Label>& labels)
{
    hideHoveredLabelToolTip();
    m_labels = labels;
    for (auto it = m_selectedLabels.begin(); it != m_selectedLabels.end();) {
        if (*it < 0 || *it >= m_labels.size()) {
            it = m_selectedLabels.erase(it);
        }
        else {
            ++it;
        }
    }
    for (auto it = m_labelTextPreviews.begin(); it != m_labelTextPreviews.end();) {
        if (it.key() < 0 || it.key() >= m_labels.size()) {
            it = m_labelTextPreviews.erase(it);
        }
        else {
            ++it;
        }
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
    setSelectedLabels(index >= 0 ? QVector<int>{index} : QVector<int>{});
}

void ImageCanvas::setSelectedLabels(QVector<int> indexes)
{
    QSet<int> selectedLabels;
    for (int index : indexes) {
        if (index >= 0 && index < m_labels.size()) {
            selectedLabels.insert(index);
        }
    }

    if (m_selectedLabels == selectedLabels) {
        return;
    }

    m_selectedLabels = std::move(selectedLabels);
    rebuildLabelItems();
}

void ImageCanvas::setLabelTextPreview(int index, const QString& text)
{
    if (index < 0 || index >= m_labels.size()) {
        return;
    }

    if (m_labelTextPreviews.value(index) == text) {
        return;
    }

    m_labelTextPreviews.insert(index, text);
    rebuildLabelItems();
}

void ImageCanvas::clearLabelTextPreview(int index)
{
    if (!m_labelTextPreviews.remove(index)) {
        return;
    }

    rebuildLabelItems();
}

void ImageCanvas::centerOnLabel(int index)
{
    if (m_pixmapItem == nullptr || index < 0 || index >= m_labels.size()) {
        return;
    }

    const QRectF rect = m_pixmapItem->boundingRect();
    const QPointF position = m_labels.at(index).position();
    centerOn(rect.left() + position.x() * rect.width(), rect.top() + position.y() * rect.height());
}

QPoint ImageCanvas::globalPositionForLabel(int index) const
{
    if (m_pixmapItem == nullptr || index < 0 || index >= m_labels.size()) {
        return viewport()->mapToGlobal(viewport()->rect().center());
    }

    const QRectF rect = m_pixmapItem->boundingRect();
    const QPointF position = m_labels.at(index).position();
    const QPoint viewportPosition =
        mapFromScene(rect.left() + position.x() * rect.width(), rect.top() + position.y() * rect.height());
    return viewport()->mapToGlobal(viewportPosition);
}

bool ImageCanvas::hasSelection() const noexcept
{
    return m_normalizedSelectionRect.width() > 0.0 && m_normalizedSelectionRect.height() > 0.0;
}

QRectF ImageCanvas::normalizedSelectionRect() const noexcept
{
    return m_normalizedSelectionRect;
}

void ImageCanvas::setZoomPercent(int percent)
{
    m_hasUserZoom = true;
    m_zoomPercent = std::clamp(percent, 10, 400);
    applyZoom();
    notifyViewportStateChanged();
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
    notifyViewportStateChanged();
}

void ImageCanvas::keyPressEvent(QKeyEvent* event)
{
    if (m_interactionMode == InteractionMode::Selection && event->matches(QKeySequence::Copy)) {
        if (copyImageToClipboard()) {
            emit imageCopiedToClipboard();
        }
        event->accept();
        return;
    }
    if (m_interactionMode == InteractionMode::Label && event->key() == Qt::Key_Delete &&
        event->modifiers() == Qt::NoModifier) {
        emit deleteRequested();
        event->accept();
        return;
    }

    QGraphicsView::keyPressEvent(event);
}

void ImageCanvas::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton) {
        setFocus();
        m_isMiddleButtonPanning = true;
        m_lastMiddlePanPosition = event->pos();
        m_pendingLabelCreate = false;
        m_pendingLabelSelect = false;
        m_pendingEmptyClick = false;
        m_pendingLabelSelectIndex = -1;
        m_pendingLabelSelectModifiers = Qt::NoModifier;
        m_pendingLabelMove = false;
        m_pendingLabelMoveIndex = -1;
        m_isSelectingRegion = false;
        hideHoveredLabelToolTip();
        viewport()->setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    if (m_interactionMode == InteractionMode::Selection && event->button() == Qt::LeftButton) {
        setFocus();
        beginSelection(event->pos());
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        setFocus();
        m_pendingLabelCreate = false;
        m_pendingLabelSelect = false;
        m_pendingEmptyClick = false;
        m_pendingLabelSelectIndex = -1;
        m_pendingLabelSelectModifiers = Qt::NoModifier;
        m_pendingLabelMove = false;
        m_pendingLabelMoveIndex = -1;
        bool pressedMarker = false;

        QGraphicsItem* item = itemAt(event->pos());
        while (item != nullptr) {
            if (item->type() == markerType) {
                auto* marker = static_cast<LabelMarkerItem*>(item);
                pressedMarker = true;
                const int labelIndex = marker->labelIndex();
                m_pendingLabelSelect = true;
                m_pendingLabelSelectIndex = labelIndex;
                m_pendingLabelSelectModifiers = event->modifiers();
                m_labelSelectPressPosition = event->pos();
                if (hasMoveLabelModifiers(event->modifiers())) {
                    m_pendingLabelMove = true;
                    m_pendingLabelMoveIndex = labelIndex;
                }
                break;
            }
            item = item->parentItem();
        }

        if (!pressedMarker) {
            m_pendingEmptyClick = true;
            m_emptyClickPressPosition = event->pos();
            if (m_pixmapItem != nullptr && m_pixmapItem->contains(mapToScene(event->pos()))) {
                m_pendingLabelCreate = true;
                m_labelCreatePressPosition = event->pos();
            }
        }
    }

    QGraphicsView::mousePressEvent(event);
}

void ImageCanvas::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (m_interactionMode == InteractionMode::Selection && event->button() == Qt::LeftButton) {
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        setFocus();
        m_pendingLabelCreate = false;

        QGraphicsItem* item = itemAt(event->pos());
        while (item != nullptr) {
            if (item->type() == markerType) {
                auto* marker = static_cast<LabelMarkerItem*>(item);
                const int labelIndex = marker->labelIndex();
                emit labelSelected(labelIndex);
                emit labelTextEditRequested(labelIndex, event->globalPosition().toPoint());
                hideHoveredLabelToolTip();
                event->accept();
                return;
            }
            item = item->parentItem();
        }
    }

    QGraphicsView::mouseDoubleClickEvent(event);
}

void ImageCanvas::mouseMoveEvent(QMouseEvent* event)
{
    if (m_isMiddleButtonPanning) {
        const QPoint delta = event->pos() - m_lastMiddlePanPosition;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        m_lastMiddlePanPosition = event->pos();
        hideHoveredLabelToolTip();
        notifyViewportStateChanged();
        event->accept();
        return;
    }

    if (m_interactionMode == InteractionMode::Selection && m_isSelectingRegion) {
        updateSelection(event->pos());
        event->accept();
        return;
    }

    if (m_isMovingLabel && m_pixmapItem != nullptr && m_movingLabelIndex >= 0 && m_movingLabelIndex < m_labels.size()) {
        m_labels[m_movingLabelIndex].setPosition(normalizedPositionFromScene(mapToScene(event->pos())));
        rebuildLabelItems();
        event->accept();
        return;
    }

    if (m_pendingLabelMove &&
        (event->pos() - m_labelSelectPressPosition).manhattanLength() >= QApplication::startDragDistance()) {
        m_isMovingLabel = true;
        m_movingLabelIndex = m_pendingLabelMoveIndex;
        m_pendingLabelMove = false;
        m_pendingLabelMoveIndex = -1;
        m_pendingLabelSelect = false;
        m_pendingLabelSelectIndex = -1;
        m_pendingLabelSelectModifiers = Qt::NoModifier;
        emit labelSelected(m_movingLabelIndex);
        hideHoveredLabelToolTip();
        event->accept();
        return;
    }

    if (m_pendingLabelCreate &&
        (event->pos() - m_labelCreatePressPosition).manhattanLength() >= QApplication::startDragDistance()) {
        m_pendingLabelCreate = false;
    }
    if (m_pendingEmptyClick &&
        (event->pos() - m_emptyClickPressPosition).manhattanLength() >= QApplication::startDragDistance()) {
        m_pendingEmptyClick = false;
    }
    if (m_pendingLabelSelect &&
        (event->pos() - m_labelSelectPressPosition).manhattanLength() >= QApplication::startDragDistance()) {
        m_pendingLabelSelect = false;
        m_pendingLabelSelectIndex = -1;
        m_pendingLabelSelectModifiers = Qt::NoModifier;
        m_pendingLabelMove = false;
        m_pendingLabelMoveIndex = -1;
    }

    QGraphicsView::mouseMoveEvent(event);
    if (m_interactionMode == InteractionMode::Label && event->buttons() == Qt::NoButton) {
        updateHoveredLabelToolTip(event->pos(), event->globalPosition().toPoint());
    }
    else {
        hideHoveredLabelToolTip();
    }
}

void ImageCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton && m_isMiddleButtonPanning) {
        m_isMiddleButtonPanning = false;
        updateCursorForInteractionMode();
        notifyViewportStateChanged();
        event->accept();
        return;
    }

    if (m_interactionMode == InteractionMode::Selection && event->button() == Qt::LeftButton && m_isSelectingRegion) {
        finishSelection(event->pos());
        event->accept();
        return;
    }

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
    const bool shouldClearLabelSelection =
        event->button() == Qt::LeftButton && m_pendingEmptyClick &&
        (event->pos() - m_emptyClickPressPosition).manhattanLength() < QApplication::startDragDistance() &&
        m_selectedLabels.size() >= 2;
    const bool shouldSelectLabel =
        event->button() == Qt::LeftButton && m_pendingLabelSelect &&
        (event->pos() - m_labelSelectPressPosition).manhattanLength() < QApplication::startDragDistance() &&
        m_pendingLabelSelectIndex >= 0 && m_pendingLabelSelectIndex < m_labels.size();

    m_pendingLabelCreate = false;
    m_pendingLabelSelect = false;
    m_pendingEmptyClick = false;
    m_pendingLabelMove = false;
    m_pendingLabelMoveIndex = -1;
    QGraphicsView::mouseReleaseEvent(event);

    if (shouldClearLabelSelection) {
        emit emptyAreaClicked();
    }
    else if (shouldCreateLabel) {
        emit labelCreateRequested(normalizedPositionFromScene(mapToScene(event->pos())));
    }
    if (shouldSelectLabel) {
        emit labelClicked(m_pendingLabelSelectIndex, m_pendingLabelSelectModifiers);
    }
    m_pendingLabelSelectIndex = -1;
    m_pendingLabelSelectModifiers = Qt::NoModifier;
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
    if (m_isMiddleButtonPanning) {
        m_isMiddleButtonPanning = false;
        updateCursorForInteractionMode();
    }
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

        const labelqt::core::LabelGroupStyle style = styleForGroup(m_labels.at(i).group());
        auto* marker = new LabelMarkerItem(i, m_selectedLabels.contains(i), style);
        const QPointF position = m_labels.at(i).position();
        marker->setPos(rect.left() + position.x() * rect.width(), rect.top() + position.y() * rect.height());
        m_scene.addItem(marker);
        m_labelItems.append(marker);

        if (m_interactionMode == InteractionMode::Label && m_selectedLabels.contains(i)) {
            auto* bubble =
                new LabelTextBubbleItem(i, displayTextForLabel(i), style, m_textBubbleFont, m_textBubbleOpacity);
            bubble->setPos(marker->pos());
            m_scene.addItem(bubble);
            m_labelItems.append(bubble);
        }
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
    notifyViewportStateChanged();
}

void ImageCanvas::notifyViewportStateChanged()
{
    if (m_isDestroying || m_pixmapItem == nullptr) {
        return;
    }
    emit viewportStateChanged(m_zoomPercent, normalizedViewCenter());
}

bool ImageCanvas::isLabelVisible(const labelqt::core::Label& label) const
{
    return !label.isDeleted() && m_visibleGroups.contains(label.group());
}

bool ImageCanvas::hasMoveLabelModifiers(Qt::KeyboardModifiers modifiers) const
{
    constexpr Qt::KeyboardModifiers relevantModifiers =
        Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier;
    return (modifiers & relevantModifiers) == m_moveLabelModifiers;
}

QString ImageCanvas::displayTextForLabel(int index) const
{
    if (m_labelTextPreviews.contains(index)) {
        return m_labelTextPreviews.value(index);
    }

    if (index < 0 || index >= m_labels.size()) {
        return {};
    }

    return m_labels.at(index).text();
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
        if (seenLabels.contains(labelIndex) || m_selectedLabels.contains(labelIndex) || labelIndex < 0 ||
            labelIndex >= m_labels.size()) {
            continue;
        }

        seenLabels.insert(labelIndex);
        const labelqt::core::Label& label = m_labels.at(labelIndex);
        if (!isLabelVisible(label)) {
            continue;
        }

        const QColor color = styleForGroup(label.group()).groupColor.isValid() ? styleForGroup(label.group()).groupColor
                                                                               : QColor(Qt::black);
        lines.append(labelBubbleHtml(labelIndex, displayTextForLabel(labelIndex), color));
    }

    if (lines.isEmpty()) {
        hideHoveredLabelToolTip();
        return;
    }

    m_hoverToolTip->setText(lines.join(QStringLiteral("<br/>")));
    m_hoverToolTip->setFont(m_textBubbleFont);
    m_hoverToolTip->setWindowOpacity(m_textBubbleOpacity);
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

QRectF ImageCanvas::imageClampedSceneRect(QPointF firstScenePosition, QPointF secondScenePosition) const
{
    if (m_pixmapItem == nullptr) {
        return {};
    }

    const QRectF imageRect = m_pixmapItem->boundingRect();
    firstScenePosition.setX(std::clamp(firstScenePosition.x(), imageRect.left(), imageRect.right()));
    firstScenePosition.setY(std::clamp(firstScenePosition.y(), imageRect.top(), imageRect.bottom()));
    secondScenePosition.setX(std::clamp(secondScenePosition.x(), imageRect.left(), imageRect.right()));
    secondScenePosition.setY(std::clamp(secondScenePosition.y(), imageRect.top(), imageRect.bottom()));

    return QRectF(firstScenePosition, secondScenePosition).normalized();
}

QRectF ImageCanvas::normalizedRectFromSceneRect(QRectF sceneRect) const
{
    if (m_pixmapItem == nullptr || sceneRect.isNull()) {
        return {};
    }

    const QRectF imageRect = m_pixmapItem->boundingRect();
    const double left = (sceneRect.left() - imageRect.left()) / imageRect.width();
    const double top = (sceneRect.top() - imageRect.top()) / imageRect.height();
    const double right = (sceneRect.right() - imageRect.left()) / imageRect.width();
    const double bottom = (sceneRect.bottom() - imageRect.top()) / imageRect.height();
    return QRectF(QPointF(std::clamp(left, 0.0, 1.0), std::clamp(top, 0.0, 1.0)),
                  QPointF(std::clamp(right, 0.0, 1.0), std::clamp(bottom, 0.0, 1.0)))
        .normalized();
}

void ImageCanvas::beginSelection(QPoint viewportPosition)
{
    if (m_pixmapItem == nullptr) {
        return;
    }

    m_pendingLabelCreate = false;
    m_pendingLabelSelect = false;
    m_pendingLabelSelectIndex = -1;
    m_isMovingLabel = false;
    m_movingLabelIndex = -1;
    hideHoveredLabelToolTip();

    m_isSelectingRegion = true;
    m_selectionStartScenePosition = mapToScene(viewportPosition);
    if (m_selectionItem == nullptr) {
        m_selectionItem = m_scene.addRect({}, QPen(QColor(46, 103, 230), 2.0, Qt::DashLine), QColor(46, 103, 230, 40));
        m_selectionItem->setZValue(8.0);
    }
    updateSelection(viewportPosition);
}

void ImageCanvas::updateSelection(QPoint viewportPosition)
{
    if (m_pixmapItem == nullptr || m_selectionItem == nullptr) {
        return;
    }

    const QRectF sceneRect = imageClampedSceneRect(m_selectionStartScenePosition, mapToScene(viewportPosition));
    m_selectionItem->setRect(sceneRect);
    m_normalizedSelectionRect = normalizedRectFromSceneRect(sceneRect);
}

void ImageCanvas::finishSelection(QPoint viewportPosition)
{
    updateSelection(viewportPosition);
    m_isSelectingRegion = false;

    if (m_selectionItem == nullptr || m_normalizedSelectionRect.width() <= 0.0 ||
        m_normalizedSelectionRect.height() <= 0.0) {
        clearSelection();
        emit selectionChanged({});
        return;
    }

    emit selectionChanged(m_normalizedSelectionRect);
}

void ImageCanvas::clearSelection()
{
    if (m_selectionItem != nullptr) {
        m_scene.removeItem(m_selectionItem);
        delete m_selectionItem;
        m_selectionItem = nullptr;
    }
    m_normalizedSelectionRect = {};
    m_isSelectingRegion = false;
}

bool ImageCanvas::copyImageToClipboard()
{
    if (m_pixmapItem == nullptr) {
        return false;
    }

    const QPixmap pixmap = m_pixmapItem->pixmap();
    if (pixmap.isNull()) {
        return false;
    }

    QRect copyRect = pixmap.rect();
    if (hasSelection()) {
        const QRectF normalizedRect = m_normalizedSelectionRect.normalized();
        const QRectF pixelRect(normalizedRect.left() * pixmap.width(), normalizedRect.top() * pixmap.height(),
                               normalizedRect.width() * pixmap.width(), normalizedRect.height() * pixmap.height());
        copyRect = pixelRect.toAlignedRect().intersected(pixmap.rect());
    }
    if (copyRect.isEmpty()) {
        return false;
    }

    QApplication::clipboard()->setPixmap(pixmap.copy(copyRect));
    return true;
}

void ImageCanvas::updateCursorForInteractionMode()
{
    if (m_interactionMode == InteractionMode::Label) {
        viewport()->setCursor(Qt::OpenHandCursor);
        return;
    }

    viewport()->unsetCursor();
}

labelqt::core::LabelGroupStyle ImageCanvas::styleForGroup(const QString& group) const
{
    const int index = static_cast<int>(m_groups.indexOf(group));
    if (index < 0 || index >= static_cast<int>(m_groupStyles.size())) {
        return {QColor(), m_markerDiameterPixels, m_markerFontPointSize, labelqt::core::MarkerShape::Circle};
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
