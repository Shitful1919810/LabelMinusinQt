#include "services/OcrProcess.h"

#include <utility>

namespace labelminus::services {

OcrProcess::OcrProcess(QObject *parent)
    : QObject(parent)
{
}

QString OcrProcess::pythonExecutable() const
{
    return m_pythonExecutable;
}

void OcrProcess::setPythonExecutable(QString executable)
{
    m_pythonExecutable = std::move(executable);
}

} // namespace labelminus::services
