#pragma once

#include "core/AppPreferences.h"
#include "core/Label.h"

#include <QColor>
#include <QGraphicsView>
#include <QSet>
#include <QStringList>
#include <QVector>

class QGraphicsItem;

class ImageCanvas final : public QGraphicsView {
    Q_OBJECT

public:
    explicit ImageCanvas(QWidget* parent = nullptr);

    void setPreferences(const labelminus::core::AppPreferences& preferences);
    void setImage(const QString& path, const QVector<labelminus::core::Label>& labels);
    void setGroups(QStringList groups);
    void setVisibleGroups(QStringList groups);
    void setSelectedLabel(int index);
    void setZoomPercent(int percent);

signals:
    void labelCreateRequested(QPointF normalizedPosition);
    void labelMoveRequested(int index, QPointF normalizedPosition);
    void labelSelected(int index);
    void undoRequested();
    void zoomPercentChanged(int percent);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void rebuildLabelItems();
    void applyZoom();
    bool isLabelVisible(const labelminus::core::Label& label) const;
    bool hasMoveLabelModifiers(Qt::KeyboardModifiers modifiers) const;
    void updateHoveredLabelToolTip(const QPoint& viewportPosition, const QPoint& globalPosition);
    void hideHoveredLabelToolTip();
    labelminus::core::LabelGroupStyle styleForGroup(const QString& group) const;
    QPointF normalizedPositionFromScene(QPointF scenePosition) const;

    QGraphicsScene m_scene;
    QGraphicsPixmapItem* m_pixmapItem{nullptr};
    QVector<labelminus::core::Label> m_labels;
    QVector<QGraphicsItem*> m_labelItems;
    QString m_imagePath;
    int m_selectedLabel{-1};
    int m_zoomPercent{100};
    double m_markerDiameterPixels{20.0};
    double m_markerFontPointSize{10.0};
    Qt::KeyboardModifiers m_moveLabelModifiers{Qt::ControlModifier};
    QStringList m_groups;
    QSet<QString> m_visibleGroups;
    QVector<labelminus::core::LabelGroupStyle> m_groupStyles;
    bool m_hasUserZoom{false};
    bool m_pendingLabelCreate{false};
    bool m_isMovingLabel{false};
    int m_movingLabelIndex{-1};
    QPoint m_labelCreatePressPosition;
};
