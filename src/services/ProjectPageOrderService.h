#pragma once

#include "core/Project.h"

#include <QVector>

namespace labelminus::services {

class ProjectPageOrderService final {
public:
    static bool isValidOrder(const QVector<int>& order, int pageCount) noexcept;
    static bool isIdentityOrder(const QVector<int>& order) noexcept;
    static QVector<labelminus::core::ImageEntry> reorderedImages(const QVector<labelminus::core::ImageEntry>& images,
                                                                 const QVector<int>& order);
    static void reorderImages(labelminus::core::Project& project, const QVector<int>& order);
};

} // namespace labelminus::services
