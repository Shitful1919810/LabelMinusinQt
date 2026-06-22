#include "ui/CanvasLabelTextEditor.h"

#include <QPlainTextEdit>
#include <QTextCursor>
#include <QVBoxLayout>

#include <algorithm>

CanvasLabelTextEditor::CanvasLabelTextEditor(QWidget* parent) : QFrame(parent)
{
    setFrameShape(QFrame::StyledPanel);
    setAutoFillBackground(true);
    setStyleSheet(QStringLiteral("QFrame {"
                                 "background: palette(base);"
                                 "border: 1px solid palette(mid);"
                                 "border-radius: 3px;"
                                 "}"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(0);

    m_editor = new QPlainTextEdit(this);
    m_editor->setFrameShape(QFrame::NoFrame);
    m_editor->setTabChangesFocus(true);
    layout->addWidget(m_editor);

    connect(m_editor, &QPlainTextEdit::textChanged, this,
            [this]() { emit textChanged(m_editor != nullptr ? m_editor->toPlainText() : QString()); });
}

void CanvasLabelTextEditor::setText(const QString& text)
{
    m_editor->setPlainText(text);
    QTextCursor cursor = m_editor->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_editor->setTextCursor(cursor);
}

QString CanvasLabelTextEditor::text() const
{
    return m_editor != nullptr ? m_editor->toPlainText() : QString();
}

void CanvasLabelTextEditor::setEditorFont(const QFont& font)
{
    m_editor->setFont(font);
}

void CanvasLabelTextEditor::moveNearGlobalPosition(const QPoint& globalPosition)
{
    QWidget* parent = parentWidget();
    if (parent == nullptr) {
        return;
    }

    const QSize viewportSize = parent->size();
    const QSize maximumSize(std::max(1, viewportSize.width() - 12), std::max(1, viewportSize.height() - 12));
    resize(QSize(320, 140)
               .boundedTo(maximumSize)
               .expandedTo(QSize(std::min(220, maximumSize.width()), std::min(96, maximumSize.height()))));

    QPoint localPosition = parent->mapFromGlobal(globalPosition + QPoint(12, 18));
    localPosition.setX(std::clamp(localPosition.x(), 0, std::max(0, viewportSize.width() - width())));
    localPosition.setY(std::clamp(localPosition.y(), 0, std::max(0, viewportSize.height() - height())));
    move(localPosition);
}

QPlainTextEdit* CanvasLabelTextEditor::editor() const noexcept
{
    return m_editor;
}
