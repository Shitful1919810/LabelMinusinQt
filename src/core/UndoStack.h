#pragma once

#include <QString>
#include <QUndoStack>

#include <functional>

namespace labelminus::core {

class UndoStack {
public:
    struct Command {
        QString text;
        std::function<void()> undo;
    };

    void push(Command command);
    void push(QString text, std::function<void()> undo);
    void push(QString text, std::function<void()> undo, std::function<void()> redo);
    bool canUndo() const noexcept;
    bool canRedo() const noexcept;
    void undo();
    void redo();
    void clear();

private:
    QUndoStack m_stack;
};

} // namespace labelminus::core
