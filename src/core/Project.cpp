#include "core/Project.h"

namespace labelminus::core {

bool Project::isEmpty() const noexcept
{
    return m_images.isEmpty();
}

void Project::clear()
{
    m_images.clear();
}

const QVector<ImageEntry> &Project::images() const noexcept
{
    return m_images;
}

QVector<ImageEntry> &Project::images() noexcept
{
    return m_images;
}

} // namespace labelminus::core

