#pragma once

#include "core/AppPreferences.h"
#include "core/Label.h"

#include <QColor>
#include <QFont>
#include <QGraphicsView>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVector>

class QGraphicsItem;
class QLabel;

class ImageCanvas final : public QGraphicsView {
    Q_OBJECT

public:
    explicit ImageCanvas(QWidget* parent = nullptr);
    ~ImageCanvas() override;

    void setPreferences(const labelminus::core::AppPreferences& preferences);
    void setImage(const QString& path, const QVector<labelminus::core::Label>& labels);
    void setLabels(const QVector<labelminus::core::Label>& labels);
    void setGroups(QStringList groups);
    void setVisibleGroups(QStringList groups);
    void setSelectedLabel(int index);
    void setSelectedLabels(QVector<int> indexes);
    void setLabelTextPreview(int index, const QString& text);
    void clearLabelTextPreview(int index);
    void centerOnLabel(int index);
    QPoint globalPositionForLabel(int index) const;
    void setZoomPercent(int percent);
    int zoomPercent() const noexcept;
    QPointF normalizedViewCenter() const;
    void restoreView(int zoomPercent, QPointF normalizedCenter);

signals:
    void labelCreateRequested(QPointF normalizedPosition);
    void labelMoveRequested(int index, QPointF normalizedPosition);
    void labelSelected(int index);
    void labelTextEditRequested(int index, QPoint globalPosition);
    void zoomPercentChanged(int percent);
    void viewportStateChanged(int zoomPercent, QPointF normalizedCenter);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void clearSceneItems();
    void rebuildLabelItems();
    void applyZoom();
    void updateScenePadding();
    void setZoomPercentAt(int percent, QPoint viewportAnchor);
    void notifyViewportStateChanged();
    bool isLabelVisible(const labelminus::core::Label& label) const;
    bool hasMoveLabelModifiers(Qt::KeyboardModifiers modifiers) const;
    QString displayTextForLabel(int index) const;
    void updateHoveredLabelToolTip(const QPoint& viewportPosition, const QPoint& globalPosition);
    void hideHoveredLabelToolTip();
    labelminus::core::LabelGroupStyle styleForGroup(const QString& group) const;
    QPointF normalizedPositionFromScene(QPointF scenePosition) const;

    QGraphicsScene m_scene;
    QGraphicsPixmapItem* m_pixmapItem{nullptr};
    QLabel* m_hoverToolTip{nullptr};
    QVector<labelminus::core::Label> m_labels;
    QVector<QGraphicsItem*> m_labelItems;
    QString m_imagePath;
    QSet<int> m_selectedLabels;
    int m_zoomPercent{100};
    double m_markerDiameterPixels{20.0};
    double m_markerFontPointSize{10.0};
    double m_textBubbleOpacity{1.0};
    QFont m_textBubbleFont;
    QHash<int, QString> m_labelTextPreviews;
    Qt::KeyboardModifiers m_moveLabelModifiers{Qt::ControlModifier};
    QStringList m_groups;
    QSet<QString> m_visibleGroups;
    QVector<labelminus::core::LabelGroupStyle> m_groupStyles;
    bool m_hasUserZoom{false};
    bool m_isDestroying{false};
    bool m_pendingLabelCreate{false};
    bool m_pendingLabelSelect{false};
    bool m_isMovingLabel{false};
    int m_pendingLabelSelectIndex{-1};
    int m_movingLabelIndex{-1};
    QPoint m_labelCreatePressPosition;
    QPoint m_labelSelectPressPosition;
};
