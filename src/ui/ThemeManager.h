#pragma once

#include <QString>
#include <QStringList>

namespace labelminus::ui {

QStringList availableApplicationThemes();
bool applyApplicationTheme(const QString& themeName);

} // namespace labelminus::ui
