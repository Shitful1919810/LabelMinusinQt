#pragma once

#include <QString>

#include <functional>
#include <vector>

namespace labelminus::core {

class UndoStack {
public:
    struct Command {
        QString text;
        std::function<void()> undo;
    };

    void push(Command command);
    void push(QString text, std::function<void()> undo);
    bool canUndo() const noexcept;
    void undo();
    void clear();

private:
    std::vector<Command> m_commands;
};

} // namespace labelminus::core
