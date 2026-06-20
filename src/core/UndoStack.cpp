#include "core/UndoStack.h"

#include <utility>

namespace labelminus::core {

void UndoStack::push(Command command)
{
    if (command.undo) {
        m_commands.push_back(std::move(command));
    }
}

void UndoStack::push(QString text, std::function<void()> undo)
{
    push(Command{std::move(text), std::move(undo)});
}

bool UndoStack::canUndo() const noexcept
{
    return !m_commands.empty();
}

void UndoStack::undo()
{
    if (m_commands.empty()) {
        return;
    }

    Command command = std::move(m_commands.back());
    m_commands.pop_back();
    command.undo();
}

void UndoStack::clear()
{
    m_commands.clear();
}

} // namespace labelminus::core
