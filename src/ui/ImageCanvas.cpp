#include "ui/ImageCanvas.h"

#include <QGraphicsPixmapItem>
#include <QPainter>
#include <QPixmap>
#include <QWheelEvent>

ImageCanvas::ImageCanvas(QWidget *parent)
    : QGraphicsView(parent)
{
    setScene(&m_scene);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
}

void ImageCanvas::openImage(const QString &path)
{
    QPixmap pixmap(path);
    m_scene.clear();
    m_pixmapItem = m_scene.addPixmap(pixmap);
    m_scene.setSceneRect(m_pixmapItem->boundingRect());
    fitInView(m_scene.sceneRect(), Qt::KeepAspectRatio);
}

void ImageCanvas::wheelEvent(QWheelEvent *event)
{
    constexpr double zoomIn = 1.15;
    constexpr double zoomOut = 1.0 / zoomIn;
    scale(event->angleDelta().y() > 0 ? zoomIn : zoomOut,
          event->angleDelta().y() > 0 ? zoomIn : zoomOut);
    event->accept();
}
