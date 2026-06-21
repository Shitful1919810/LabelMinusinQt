#include "core/UndoStack.h"

#include <QUndoCommand>

#include <utility>

namespace labelminus::core {

namespace {
class CallbackUndoCommand final : public QUndoCommand {
public:
    CallbackUndoCommand(QString text, std::function<void()> undo, std::function<void()> redo)
        : QUndoCommand(std::move(text)), m_undo(std::move(undo)), m_redo(std::move(redo))
    {
    }

    void undo() override
    {
        if (m_undo) {
            m_undo();
        }
    }

    void redo() override
    {
        if (m_skipInitialRedo) {
            m_skipInitialRedo = false;
            return;
        }

        if (m_redo) {
            m_redo();
        }
    }

private:
    std::function<void()> m_undo;
    std::function<void()> m_redo;
    bool m_skipInitialRedo{true};
};
} // namespace

void UndoStack::push(Command command)
{
    push(std::move(command.text), std::move(command.undo), {});
}

void UndoStack::push(QString text, std::function<void()> undo)
{
    push(std::move(text), std::move(undo), {});
}

void UndoStack::push(QString text, std::function<void()> undo, std::function<void()> redo)
{
    if (undo || redo) {
        m_stack.push(new CallbackUndoCommand(std::move(text), std::move(undo), std::move(redo)));
    }
}

bool UndoStack::canUndo() const noexcept
{
    return m_stack.canUndo();
}

bool UndoStack::canRedo() const noexcept
{
    return m_stack.canRedo();
}

void UndoStack::undo()
{
    m_stack.undo();
}

void UndoStack::redo()
{
    m_stack.redo();
}

void UndoStack::clear()
{
    m_stack.clear();
}

} // namespace labelminus::core
