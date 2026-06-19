#include "core/AppPreferences.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QStringList>

#include <algorithm>
#include <cmath>

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
                                   double maximum, AppPreferenceWarningType wrongType,
                                   AppPreferenceWarningType outOfRange, const QString& displayPrefix,
                                   QVector<AppPreferenceWarning>& warnings)
{
    const QJsonValue value = object.value(key);
    if (value.isUndefined()) {
        return fallback;
    }

    if (!value.isDouble()) {
        warnings.append(makeWarning(wrongType, QStringLiteral("%1.%2").arg(displayPrefix, key)));
        return fallback;
    }

    const double size = value.toDouble();
    if (size <= 0.0) {
        warnings.append(makeWarning(outOfRange, QStringLiteral("%1.%2").arg(displayPrefix, key)));
        return fallback;
    }

    return std::clamp(size, minimum, maximum);
}

int positiveIntegerFromJsonValue(const QJsonObject& object, const QString& key, const QString& displayKey, int fallback,
                                 int minimum, int maximum, AppPreferenceWarningType wrongType,
                                 AppPreferenceWarningType outOfRange, QVector<AppPreferenceWarning>& warnings)
{
    const QJsonValue value = object.value(key);
    if (value.isUndefined()) {
        return fallback;
    }

    if (!value.isDouble()) {
        warnings.append(makeWarning(wrongType, displayKey));
        return fallback;
    }

    const double number = value.toDouble();
    if (number <= 0.0 || std::floor(number) != number) {
        warnings.append(makeWarning(outOfRange, displayKey));
        return fallback;
    }

    return std::clamp(static_cast<int>(number), minimum, maximum);
}

MarkerShape markerShapeFromString(const QString& markerStyle, MarkerShape fallback, qsizetype index,
                                  QVector<AppPreferenceWarning>& warnings)
{
    if (markerStyle == QStringLiteral("circle")) {
        return MarkerShape::Circle;
    }
    if (markerStyle == QStringLiteral("square")) {
        return MarkerShape::Square;
    }

    warnings.append(makeWarning(AppPreferenceWarningType::GroupStyleMarkerStyleInvalid, {}, {}, index));
    return fallback;
}

Qt::KeyboardModifiers modifiersFromString(const QString& text, Qt::KeyboardModifiers fallback,
                                          QVector<AppPreferenceWarning>& warnings)
{
    Qt::KeyboardModifiers modifiers;
    const QStringList parts = text.toLower().split(QLatin1Char('+'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        warnings.append(
            makeWarning(AppPreferenceWarningType::MoveLabelModifierInvalid, QStringLiteral("input.moveLabelModifier")));
        return fallback;
    }

    for (const QString& rawPart : parts) {
        const QString part = rawPart.trimmed();
        if (part == QStringLiteral("none")) {
            if (parts.size() == 1) {
                return Qt::NoModifier;
            }
            warnings.append(makeWarning(AppPreferenceWarningType::MoveLabelModifierInvalid,
                                        QStringLiteral("input.moveLabelModifier")));
            return fallback;
        }
        if (part == QStringLiteral("ctrl") || part == QStringLiteral("control")) {
            modifiers |= Qt::ControlModifier;
        }
        else if (part == QStringLiteral("shift")) {
            modifiers |= Qt::ShiftModifier;
        }
        else if (part == QStringLiteral("alt")) {
            modifiers |= Qt::AltModifier;
        }
        else if (part == QStringLiteral("meta") || part == QStringLiteral("super") || part == QStringLiteral("cmd")) {
            modifiers |= Qt::MetaModifier;
        }
        else {
            warnings.append(makeWarning(AppPreferenceWarningType::MoveLabelModifierInvalid,
                                        QStringLiteral("input.moveLabelModifier")));
            return fallback;
        }
    }

    return modifiers;
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
                labelMarker, QStringLiteral("diameter"), preferences.m_labelMarkerDiameterPixels, 1.0, 256.0,
                AppPreferenceWarningType::MarkerSizeWrongType, AppPreferenceWarningType::MarkerSizeOutOfRange,
                QStringLiteral("labelMarker"), warnings);
            preferences.m_labelMarkerFontPointSize = positiveNumberFromJsonValue(
                labelMarker, QStringLiteral("fontPointSize"), preferences.m_labelMarkerFontPointSize, 0.1, 256.0,
                AppPreferenceWarningType::MarkerSizeWrongType, AppPreferenceWarningType::MarkerSizeOutOfRange,
                QStringLiteral("labelMarker"), warnings);
        }
    }

    const QJsonValue labelTableValue = root.value(QStringLiteral("labelTable"));
    if (!labelTableValue.isUndefined()) {
        if (!labelTableValue.isObject()) {
            warnings.append(makeWarning(AppPreferenceWarningType::LabelTableNotObject));
        }
        else {
            const QJsonObject labelTable = labelTableValue.toObject();
            preferences.m_labelTableMaxTextRows = positiveIntegerFromJsonValue(
                labelTable, QStringLiteral("maxTextRows"), QStringLiteral("labelTable.maxTextRows"),
                preferences.m_labelTableMaxTextRows, 1, 50, AppPreferenceWarningType::LabelTableMaxTextRowsWrongType,
                AppPreferenceWarningType::LabelTableMaxTextRowsOutOfRange, warnings);
        }
    }

    const QJsonValue inputValue = root.value(QStringLiteral("input"));
    if (!inputValue.isUndefined()) {
        if (!inputValue.isObject()) {
            warnings.append(makeWarning(AppPreferenceWarningType::InputNotObject));
        }
        else {
            const QJsonValue moveLabelModifierValue = inputValue.toObject().value(QStringLiteral("moveLabelModifier"));
            if (!moveLabelModifierValue.isUndefined()) {
                if (moveLabelModifierValue.isString()) {
                    preferences.m_moveLabelModifiers = modifiersFromString(moveLabelModifierValue.toString(),
                                                                           preferences.m_moveLabelModifiers, warnings);
                }
                else {
                    warnings.append(makeWarning(AppPreferenceWarningType::MoveLabelModifierInvalid,
                                                QStringLiteral("input.moveLabelModifier")));
                }
            }
        }
    }

    const QJsonValue groupStylesValue = root.value(QStringLiteral("groupStyles"));
    if (!groupStylesValue.isUndefined()) {
        if (!groupStylesValue.isArray()) {
            warnings.append(makeWarning(AppPreferenceWarningType::GroupStylesNotArray));
        }
        else {
            const QJsonArray groupStyles = groupStylesValue.toArray();
            for (qsizetype i = 0; i < groupStyles.size(); ++i) {
                if (!groupStyles.at(i).isObject()) {
                    warnings.append(makeWarning(AppPreferenceWarningType::GroupStyleNotObject, {}, {}, i));
                    preferences.m_groupStyles.append(LabelGroupStyle{});
                    continue;
                }

                const QJsonObject groupStyleObject = groupStyles.at(i).toObject();
                LabelGroupStyle groupStyle;
                groupStyle.markerDiameter = preferences.m_labelMarkerDiameterPixels;
                groupStyle.fontPointSize = preferences.m_labelMarkerFontPointSize;

                const QJsonValue colorValue = groupStyleObject.value(QStringLiteral("groupColor"));
                if (!colorValue.isUndefined()) {
                    const QColor color = colorFromJsonValue(colorValue);
                    if (color.isValid()) {
                        groupStyle.groupColor = color;
                    }
                    else {
                        warnings.append(makeWarning(AppPreferenceWarningType::InvalidGroupStyleColor, {}, {}, i));
                    }
                }

                groupStyle.markerDiameter = positiveNumberFromJsonValue(
                    groupStyleObject, QStringLiteral("markerDiameter"), groupStyle.markerDiameter, 1.0, 256.0,
                    AppPreferenceWarningType::GroupStyleMarkerSizeWrongType,
                    AppPreferenceWarningType::GroupStyleMarkerSizeOutOfRange, QStringLiteral("groupStyles[%1]").arg(i),
                    warnings);
                groupStyle.fontPointSize = positiveNumberFromJsonValue(
                    groupStyleObject, QStringLiteral("fontPointSize"), groupStyle.fontPointSize, 0.1, 256.0,
                    AppPreferenceWarningType::GroupStyleMarkerSizeWrongType,
                    AppPreferenceWarningType::GroupStyleMarkerSizeOutOfRange, QStringLiteral("groupStyles[%1]").arg(i),
                    warnings);

                const QJsonValue markerStyleValue = groupStyleObject.value(QStringLiteral("markerStyle"));
                if (!markerStyleValue.isUndefined()) {
                    if (markerStyleValue.isString()) {
                        groupStyle.markerShape =
                            markerShapeFromString(markerStyleValue.toString(), groupStyle.markerShape, i, warnings);
                    }
                    else {
                        warnings.append(makeWarning(AppPreferenceWarningType::GroupStyleMarkerStyleInvalid, {}, {}, i));
                    }
                }

                preferences.m_groupStyles.append(groupStyle);
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

int AppPreferences::labelTableMaxTextRows() const noexcept
{
    return m_labelTableMaxTextRows;
}

Qt::KeyboardModifiers AppPreferences::moveLabelModifiers() const noexcept
{
    return m_moveLabelModifiers;
}

const QVector<LabelGroupStyle>& AppPreferences::groupStyles() const noexcept
{
    return m_groupStyles;
}

} // namespace labelminus::core
