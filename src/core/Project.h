#pragma once

#include "core/Label.h"

#include <QString>
#include <QVector>

namespace labelminus::core {

class ImageEntry
{
public:
    QString path;
    QVector<Label> labels;
};

class Project
{
public:
    bool isEmpty() const noexcept;
    void clear();

    const QVector<ImageEntry> &images() const noexcept;
    QVector<ImageEntry> &images() noexcept;

private:
    QVector<ImageEntry> m_images;
};

} // namespace labelminus::core

