#include "core/AppPreferences.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <algorithm>

namespace labelminus::core {

namespace {
QString preferencePath()
{
    const QString localPath = QDir::current().filePath(QStringLiteral("preference.json"));
    if (QFile::exists(localPath)) {
        return localPath;
    }
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("preference.json"));
}

QColor colorFromJsonValue(const QJsonValue& value)
{
    if (value.isString()) {
        const QColor color(value.toString());
        return color.isValid() ? color : QColor();
    }

    if (value.isDouble()) {
        const auto rgb = static_cast<QRgb>(value.toInt());
        return QColor::fromRgb(rgb);
    }

    return {};
}
} // namespace

AppPreferences AppPreferences::load()
{
    AppPreferences preferences;

    QFile file(preferencePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return preferences;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return preferences;
    }

    const QJsonObject root = document.object();
    const QJsonObject labelMarker = root.value(QStringLiteral("labelMarker")).toObject();
    preferences.m_labelMarkerDiameter =
        labelMarker.value(QStringLiteral("diameter")).toInt(preferences.m_labelMarkerDiameter);
    preferences.m_labelMarkerFontPointSize =
        labelMarker.value(QStringLiteral("fontPointSize")).toInt(preferences.m_labelMarkerFontPointSize);

    preferences.m_labelMarkerDiameter = std::clamp(preferences.m_labelMarkerDiameter, 12, 96);
    preferences.m_labelMarkerFontPointSize = std::clamp(preferences.m_labelMarkerFontPointSize, 6, 32);

    const QJsonArray groupColors = root.value(QStringLiteral("groupColors")).toArray();
    for (const QJsonValue& value : groupColors) {
        const QColor color = colorFromJsonValue(value);
        if (color.isValid()) {
            preferences.m_groupColors.append(color);
        }
    }

    return preferences;
}

int AppPreferences::labelMarkerDiameter() const noexcept
{
    return m_labelMarkerDiameter;
}

int AppPreferences::labelMarkerFontPointSize() const noexcept
{
    return m_labelMarkerFontPointSize;
}

const QVector<QColor>& AppPreferences::groupColors() const noexcept
{
    return m_groupColors;
}

} // namespace labelminus::core
