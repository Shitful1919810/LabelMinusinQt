#include "core/Project.h"

#include <utility>

namespace labelminus::core {

bool Project::isEmpty() const noexcept
{
    return m_images.isEmpty();
}

void Project::clear()
{
    m_images.clear();
    m_groups.clear();
    m_filePath.clear();
    m_sourceName.clear();
}

const QVector<ImageEntry>& Project::images() const noexcept
{
    return m_images;
}

QVector<ImageEntry>& Project::images() noexcept
{
    return m_images;
}

const QStringList& Project::groups() const noexcept
{
    return m_groups;
}

QStringList& Project::groups() noexcept
{
    return m_groups;
}

void Project::setGroups(QStringList groups)
{
    m_groups = std::move(groups);
}

QString Project::filePath() const
{
    return m_filePath;
}

void Project::setFilePath(QString filePath)
{
    m_filePath = std::move(filePath);
}

QString Project::sourceName() const
{
    return m_sourceName;
}

void Project::setSourceName(QString sourceName)
{
    m_sourceName = std::move(sourceName);
}

} // namespace labelminus::core
