#pragma once

#include "core/Label.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace labelminus::core {

class ImageEntry {
public:
    QString name;
    QString path;
    QVector<Label> labels;
};

class Project {
public:
    bool isEmpty() const noexcept;
    void clear();

    const QVector<ImageEntry>& images() const noexcept;
    QVector<ImageEntry>& images() noexcept;

    const QStringList& groups() const noexcept;
    QStringList& groups() noexcept;
    void setGroups(QStringList groups);

    QString filePath() const;
    void setFilePath(QString filePath);

    QString sourceName() const;
    void setSourceName(QString sourceName);

private:
    QVector<ImageEntry> m_images;
    QStringList m_groups;
    QString m_filePath;
    QString m_sourceName;
};

} // namespace labelminus::core
