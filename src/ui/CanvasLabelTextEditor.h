#pragma once

#include <QFrame>

class QPlainTextEdit;
class QGraphicsOpacityEffect;

class CanvasLabelTextEditor final : public QFrame {
    Q_OBJECT

public:
    explicit CanvasLabelTextEditor(QWidget* parent = nullptr);

    void setText(const QString& text);
    QString text() const;
    void setEditorFont(const QFont& font);
    void setEditorOpacity(double opacity);
    void moveNearGlobalPosition(const QPoint& globalPosition);
    QPlainTextEdit* editor() const noexcept;

signals:
    void textChanged(QString text);

private:
    QPlainTextEdit* m_editor{nullptr};
    QGraphicsOpacityEffect* m_opacityEffect{nullptr};
};
