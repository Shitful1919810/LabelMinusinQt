#pragma once

#include "core/AppPreferences.h"

#include <QFont>
#include <QGraphicsItem>
#include <QString>
#include <QTextDocument>

int canvasLabelMarkerItemType();
QString canvasLabelBubbleHtml(int labelIndex, const QString& text, const QColor& color);

class CanvasLabelMarkerItem final : public QGraphicsItem {
public:
    CanvasLabelMarkerItem(int labelIndex, bool selected, labelqt::core::LabelGroupStyle style,
                          QGraphicsItem* parent = nullptr);

    int type() const override;
    int labelIndex() const noexcept;
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    int m_labelIndex;
    bool m_selected;
    labelqt::core::LabelGroupStyle m_style;
};

class CanvasLabelTextBubbleItem final : public QGraphicsItem {
public:
    CanvasLabelTextBubbleItem(int labelIndex, QString text, labelqt::core::LabelGroupStyle style, QFont bubbleFont,
                              double opacity, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    void rebuildDocument();

    int m_labelIndex;
    QString m_text;
    labelqt::core::LabelGroupStyle m_style;
    QFont m_bubbleFont;
    double m_opacity{1.0};
    QTextDocument m_document;
};
