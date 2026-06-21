#include "ui/ThemeManager.h"

#include "core/ApplicationTheme.h"

#include <QApplication>
#include <QFile>
#include <QTextStream>

namespace labelminus::ui {

QStringList availableApplicationThemes()
{
    return labelminus::core::builtInApplicationThemeIds();
}

bool applyApplicationTheme(const QString& themeName)
{
    const QString normalizedTheme = themeName.trimmed();
    if (normalizedTheme.isEmpty() || normalizedTheme == QStringLiteral("none") ||
        normalizedTheme == QStringLiteral("system")) {
        qApp->setStyleSheet({});
        return true;
    }

    const QString resourcePath = labelminus::core::applicationThemeResourcePath(normalizedTheme);
    if (resourcePath.isEmpty()) {
        return false;
    }

    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);
    qApp->setStyleSheet(stream.readAll());
    return true;
}

} // namespace labelminus::ui
