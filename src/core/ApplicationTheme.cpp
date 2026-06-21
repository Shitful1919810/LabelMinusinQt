#include "core/ApplicationTheme.h"

namespace labelminus::core {

const QVector<ApplicationTheme>& builtInApplicationThemes()
{
    static const QVector<ApplicationTheme> themes{
        {QStringLiteral("breezeDark"), QStringLiteral(":/dark/stylesheet.qss")},
        {QStringLiteral("breezeLight"), QStringLiteral(":/light/stylesheet.qss")},
        {QStringLiteral("breezeDarkBlue"), QStringLiteral(":/dark-blue/stylesheet.qss")},
        {QStringLiteral("breezeLightBlue"), QStringLiteral(":/light-blue/stylesheet.qss")},
    };
    return themes;
}

QStringList builtInApplicationThemeIds()
{
    QStringList ids;
    ids.reserve(builtInApplicationThemes().size());
    for (const ApplicationTheme& theme : builtInApplicationThemes()) {
        ids.append(theme.id);
    }
    return ids;
}

QString applicationThemeResourcePath(const QString& themeId)
{
    for (const ApplicationTheme& theme : builtInApplicationThemes()) {
        if (theme.id == themeId) {
            return theme.resourcePath;
        }
    }
    return {};
}

bool isBuiltInApplicationTheme(const QString& themeId)
{
    return themeId.isEmpty() || themeId == QStringLiteral("none") || themeId == QStringLiteral("system") ||
           !applicationThemeResourcePath(themeId).isEmpty();
}

} // namespace labelminus::core
