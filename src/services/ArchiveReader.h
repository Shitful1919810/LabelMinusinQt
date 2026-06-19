#pragma once

#include <QString>
#include <QStringList>

namespace labelminus::services {

class ArchiveReader
{
public:
    static bool isAvailable() noexcept;
    QStringList listImages(const QString &archivePath) const;
};

} // namespace labelminus::services

