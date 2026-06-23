#include "services/ProjectPageOrderService.h"

#include <QSet>

namespace labelminus::services {

bool ProjectPageOrderService::isValidOrder(const QVector<int>& order, int pageCount) noexcept
{
    if (pageCount < 0 || order.size() > pageCount) {
        return false;
    }

    QSet<int> seen;
    seen.reserve(pageCount);
    for (int index : order) {
        if (index < 0 || index >= pageCount || seen.contains(index)) {
            return false;
        }
        seen.insert(index);
    }
    return true;
}

bool ProjectPageOrderService::isIdentityOrder(const QVector<int>& order) noexcept
{
    for (int i = 0; i < order.size(); ++i) {
        if (order.at(i) != i) {
            return false;
        }
    }
    return true;
}

QVector<labelminus::core::ImageEntry>
ProjectPageOrderService::reorderedImages(const QVector<labelminus::core::ImageEntry>& images, const QVector<int>& order)
{
    if (!isValidOrder(order, static_cast<int>(images.size()))) {
        return images;
    }

    QVector<labelminus::core::ImageEntry> reordered;
    reordered.reserve(images.size());
    for (int sourceIndex : order) {
        reordered.append(images.at(sourceIndex));
    }
    return reordered;
}

void ProjectPageOrderService::reorderImages(labelminus::core::Project& project, const QVector<int>& order)
{
    if (!isValidOrder(order, static_cast<int>(project.images().size())) || isIdentityOrder(order)) {
        return;
    }

    project.images() = reorderedImages(project.images(), order);
}

} // namespace labelminus::services
