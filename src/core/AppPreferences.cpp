#include "core/AppPreferences.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
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

AppPreferenceWarning makeWarning(AppPreferenceWarningType type, QString key = {}, QString detail = {},
                                 qsizetype index = -1)
{
    return AppPreferenceWarning{type, std::move(key), std::move(detail), index};
}

double positiveNumberFromJsonValue(const QJsonObject& object, const QString& key, double fallback, double minimum,
                                   double maximum, QVector<AppPreferenceWarning>& warnings)
{
    const QJsonValue value = object.value(key);
    if (value.isUndefined()) {
        return fallback;
    }

    if (!value.isDouble()) {
        warnings.append(
            makeWarning(AppPreferenceWarningType::MarkerSizeWrongType, QStringLiteral("labelMarker.%1").arg(key)));
        return fallback;
    }

    const double size = value.toDouble();
    if (size <= 0.0) {
        warnings.append(
            makeWarning(AppPreferenceWarningType::MarkerSizeOutOfRange, QStringLiteral("labelMarker.%1").arg(key)));
        return fallback;
    }

    return std::clamp(size, minimum, maximum);
}
} // namespace

AppPreferences AppPreferences::load()
{
    return loadWithDiagnostics().preferences;
}

AppPreferencesLoadResult AppPreferences::loadWithDiagnostics()
{
    return loadFromFile(preferencePath());
}

AppPreferencesLoadResult AppPreferences::loadFromFile(const QString& path)
{
    AppPreferences preferences;
    QVector<AppPreferenceWarning> warnings;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        warnings.append(makeWarning(AppPreferenceWarningType::FileNotReadable));
        return {preferences, warnings};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        warnings.append(makeWarning(AppPreferenceWarningType::InvalidJson, {}, parseError.errorString()));
        return {preferences, warnings};
    }

    if (!document.isObject()) {
        warnings.append(makeWarning(AppPreferenceWarningType::RootNotObject));
        return {preferences, warnings};
    }

    const QJsonObject root = document.object();
    const QJsonValue labelMarkerValue = root.value(QStringLiteral("labelMarker"));
    if (!labelMarkerValue.isUndefined()) {
        if (!labelMarkerValue.isObject()) {
            warnings.append(makeWarning(AppPreferenceWarningType::LabelMarkerNotObject));
        }
        else {
            const QJsonObject labelMarker = labelMarkerValue.toObject();
            preferences.m_labelMarkerDiameterPixels = positiveNumberFromJsonValue(
                labelMarker, QStringLiteral("diameter"), preferences.m_labelMarkerDiameterPixels, 1.0, 256.0, warnings);
            preferences.m_labelMarkerFontPointSize =
                positiveNumberFromJsonValue(labelMarker, QStringLiteral("fontPointSize"),
                                            preferences.m_labelMarkerFontPointSize, 0.1, 256.0, warnings);
        }
    }

    const QJsonValue groupColorsValue = root.value(QStringLiteral("groupColors"));
    if (!groupColorsValue.isUndefined()) {
        if (!groupColorsValue.isArray()) {
            warnings.append(makeWarning(AppPreferenceWarningType::GroupColorsNotArray));
        }
        else {
            const QJsonArray groupColors = groupColorsValue.toArray();
            for (qsizetype i = 0; i < groupColors.size(); ++i) {
                const QColor color = colorFromJsonValue(groupColors.at(i));
                if (color.isValid()) {
                    preferences.m_groupColors.append(color);
                }
                else {
                    warnings.append(makeWarning(AppPreferenceWarningType::InvalidGroupColor, {}, {}, i));
                }
            }
        }
    }

    return {preferences, warnings};
}

double AppPreferences::labelMarkerDiameterPixels() const noexcept
{
    return m_labelMarkerDiameterPixels;
}

double AppPreferences::labelMarkerFontPointSize() const noexcept
{
    return m_labelMarkerFontPointSize;
}

const QVector<QColor>& AppPreferences::groupColors() const noexcept
{
    return m_groupColors;
}

} // namespace labelminus::core
