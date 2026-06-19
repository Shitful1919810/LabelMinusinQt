#pragma once

#include <QObject>
#include <QString>

namespace labelminus::services {

class OcrProcess final : public QObject
{
    Q_OBJECT

public:
    explicit OcrProcess(QObject *parent = nullptr);

    QString pythonExecutable() const;
    void setPythonExecutable(QString executable);

private:
    QString m_pythonExecutable{"python3"};
};

} // namespace labelminus::services

