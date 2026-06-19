#pragma once

#include <QGraphicsView>

class ImageCanvas final : public QGraphicsView
{
    Q_OBJECT

public:
    explicit ImageCanvas(QWidget *parent = nullptr);

    void openImage(const QString &path);

protected:
    void wheelEvent(QWheelEvent *event) override;

private:
    QGraphicsScene m_scene;
    QGraphicsPixmapItem *m_pixmapItem{nullptr};
};

