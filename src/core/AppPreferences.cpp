#include "core/AppPreferences.h"

#include "core/ApplicationTheme.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QKeySequence>
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

double nonNegativeNumberFromJsonValue(const QJsonObject& object, const QString& key, const QString& displayKey,
                                      double fallback, double maximum, AppPreferenceWarningType wrongType,
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
    if (number < 0.0) {
        warnings.append(makeWarning(outOfRange, displayKey));
        return fallback;
    }

    return std::clamp(number, 0.0, maximum);
}

double boundedNumberFromJsonValue(const QJsonObject& object, const QString& key, const QString& displayKey,
                                  double fallback, double minimum, double maximum, AppPreferenceWarningType wrongType,
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
    if (number < minimum || number > maximum) {
        warnings.append(makeWarning(outOfRange, displayKey));
        return fallback;
    }

    return number;
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

Qt::KeyboardModifiers modifiersFromString(const QString& text, const QString& key, Qt::KeyboardModifiers fallback,
                                          AppPreferenceWarningType warningType, QVector<AppPreferenceWarning>& warnings)
{
    Qt::KeyboardModifiers modifiers;
    const QStringList parts = text.toLower().split(QLatin1Char('+'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        warnings.append(makeWarning(warningType, key));
        return fallback;
    }

    for (const QString& rawPart : parts) {
        const QString part = rawPart.trimmed();
        if (part == QStringLiteral("none")) {
            if (parts.size() == 1) {
                return Qt::NoModifier;
            }
            warnings.append(makeWarning(warningType, key));
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
            warnings.append(makeWarning(warningType, key));
            return fallback;
        }
    }

    return modifiers;
}

QKeySequence keySequenceFromJsonValue(const QJsonValue& value, const QString& key, const QKeySequence& fallback,
                                      AppPreferenceWarningType invalidType, QVector<AppPreferenceWarning>& warnings)
{
    if (value.isUndefined()) {
        return fallback;
    }

    if (!value.isString()) {
        warnings.append(makeWarning(invalidType, key));
        return fallback;
    }

    const QString text = value.toString().trimmed();
    const QKeySequence sequence = QKeySequence::fromString(text, QKeySequence::PortableText);
    if (sequence.isEmpty()) {
        warnings.append(makeWarning(invalidType, key));
        return fallback;
    }

    return sequence;
}

QString modifiersToString(Qt::KeyboardModifiers modifiers)
{
    if (modifiers == Qt::NoModifier) {
        return QStringLiteral("none");
    }

    QStringList parts;
    if (modifiers.testFlag(Qt::ControlModifier)) {
        parts.append(QStringLiteral("ctrl"));
    }
    if (modifiers.testFlag(Qt::ShiftModifier)) {
        parts.append(QStringLiteral("shift"));
    }
    if (modifiers.testFlag(Qt::AltModifier)) {
        parts.append(QStringLiteral("alt"));
    }
    if (modifiers.testFlag(Qt::MetaModifier)) {
        parts.append(QStringLiteral("meta"));
    }
    return parts.join(QLatin1Char('+'));
}

QString markerShapeToString(MarkerShape markerShape)
{
    return markerShape == MarkerShape::Square ? QStringLiteral("square") : QStringLiteral("circle");
}
} // namespace

AppPreferencesLoadResult AppPreferences::loadFromDocument(const QJsonDocument& document,
                                                          const QJsonParseError* parseError)
{
    AppPreferences preferences;
    QVector<AppPreferenceWarning> warnings;

    if (parseError != nullptr && parseError->error != QJsonParseError::NoError) {
        warnings.append(makeWarning(AppPreferenceWarningType::InvalidJson, {}, parseError->errorString()));
        return {preferences, warnings};
    }

    if (!document.isObject()) {
        warnings.append(makeWarning(AppPreferenceWarningType::RootNotObject));
        return {preferences, warnings};
    }

    const QJsonObject root = document.object();
    const QJsonValue appearanceValue = root.value(QStringLiteral("appearance"));
    if (!appearanceValue.isUndefined()) {
        if (!appearanceValue.isObject()) {
            warnings.append(makeWarning(AppPreferenceWarningType::AppearanceNotObject));
        }
        else {
            const QJsonValue styleValue = appearanceValue.toObject().value(QStringLiteral("style"));
            if (!styleValue.isUndefined()) {
                if (styleValue.isString()) {
                    preferences.m_applicationStyle = styleValue.toString().trimmed();
                }
                else {
                    warnings.append(makeWarning(AppPreferenceWarningType::AppearanceStyleWrongType,
                                                QStringLiteral("appearance.style")));
                }
            }

            const QJsonValue themeValue = appearanceValue.toObject().value(QStringLiteral("theme"));
            if (!themeValue.isUndefined()) {
                if (themeValue.isString()) {
                    const QString applicationTheme = themeValue.toString().trimmed();
                    if (isBuiltInApplicationTheme(applicationTheme)) {
                        preferences.m_applicationTheme = applicationTheme;
                    }
                    else {
                        warnings.append(makeWarning(AppPreferenceWarningType::AppearanceThemeWrongType,
                                                    QStringLiteral("appearance.theme")));
                    }
                }
                else {
                    warnings.append(makeWarning(AppPreferenceWarningType::AppearanceThemeWrongType,
                                                QStringLiteral("appearance.theme")));
                }
            }
        }
    }

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

            const QJsonValue fontFamilyValue = labelTable.value(QStringLiteral("fontFamily"));
            if (!fontFamilyValue.isUndefined()) {
                if (fontFamilyValue.isString()) {
                    preferences.m_labelTableFontFamily = fontFamilyValue.toString().trimmed();
                }
                else {
                    warnings.append(makeWarning(AppPreferenceWarningType::LabelTableFontFamilyWrongType,
                                                QStringLiteral("labelTable.fontFamily")));
                }
            }

            preferences.m_labelTableFontPointSize = nonNegativeNumberFromJsonValue(
                labelTable, QStringLiteral("fontPointSize"), QStringLiteral("labelTable.fontPointSize"),
                preferences.m_labelTableFontPointSize, 256.0,
                AppPreferenceWarningType::LabelTableFontPointSizeWrongType,
                AppPreferenceWarningType::LabelTableFontPointSizeOutOfRange, warnings);
        }
    }

    const QJsonValue labelTextEditorValue = root.value(QStringLiteral("labelTextEditor"));
    if (!labelTextEditorValue.isUndefined()) {
        if (!labelTextEditorValue.isObject()) {
            warnings.append(makeWarning(AppPreferenceWarningType::LabelTextEditorNotObject));
        }
        else {
            const QJsonObject labelTextEditor = labelTextEditorValue.toObject();
            const QJsonValue fontFamilyValue = labelTextEditor.value(QStringLiteral("fontFamily"));
            if (!fontFamilyValue.isUndefined()) {
                if (fontFamilyValue.isString()) {
                    preferences.m_labelTextEditorFontFamily = fontFamilyValue.toString().trimmed();
                }
                else {
                    warnings.append(makeWarning(AppPreferenceWarningType::LabelTextEditorFontFamilyWrongType,
                                                QStringLiteral("labelTextEditor.fontFamily")));
                }
            }

            preferences.m_labelTextEditorFontPointSize = nonNegativeNumberFromJsonValue(
                labelTextEditor, QStringLiteral("fontPointSize"), QStringLiteral("labelTextEditor.fontPointSize"),
                preferences.m_labelTextEditorFontPointSize, 256.0,
                AppPreferenceWarningType::LabelTextEditorFontPointSizeWrongType,
                AppPreferenceWarningType::LabelTextEditorFontPointSizeOutOfRange, warnings);
        }
    }

    const QJsonValue markerTextBubbleValue = root.value(QStringLiteral("markerTextBubble"));
    if (!markerTextBubbleValue.isUndefined()) {
        if (!markerTextBubbleValue.isObject()) {
            warnings.append(makeWarning(AppPreferenceWarningType::MarkerTextBubbleNotObject));
        }
        else {
            const QJsonObject markerTextBubble = markerTextBubbleValue.toObject();
            const QJsonValue fontFamilyValue = markerTextBubble.value(QStringLiteral("fontFamily"));
            if (!fontFamilyValue.isUndefined()) {
                if (fontFamilyValue.isString()) {
                    preferences.m_markerTextBubbleFontFamily = fontFamilyValue.toString().trimmed();
                }
                else {
                    warnings.append(makeWarning(AppPreferenceWarningType::MarkerTextBubbleFontFamilyWrongType,
                                                QStringLiteral("markerTextBubble.fontFamily")));
                }
            }

            preferences.m_markerTextBubbleFontPointSize = nonNegativeNumberFromJsonValue(
                markerTextBubble, QStringLiteral("fontPointSize"), QStringLiteral("markerTextBubble.fontPointSize"),
                preferences.m_markerTextBubbleFontPointSize, 256.0,
                AppPreferenceWarningType::MarkerTextBubbleFontPointSizeWrongType,
                AppPreferenceWarningType::MarkerTextBubbleFontPointSizeOutOfRange, warnings);
            preferences.m_markerTextBubbleOpacity = boundedNumberFromJsonValue(
                markerTextBubble, QStringLiteral("opacity"), QStringLiteral("markerTextBubble.opacity"),
                preferences.m_markerTextBubbleOpacity, 0.0, 1.0,
                AppPreferenceWarningType::MarkerTextBubbleOpacityWrongType,
                AppPreferenceWarningType::MarkerTextBubbleOpacityOutOfRange, warnings);
        }
    }

    const QJsonValue canvasLabelTextEditorValue = root.value(QStringLiteral("canvasLabelTextEditor"));
    if (!canvasLabelTextEditorValue.isUndefined()) {
        if (!canvasLabelTextEditorValue.isObject()) {
            warnings.append(makeWarning(AppPreferenceWarningType::CanvasLabelTextEditorNotObject));
        }
        else {
            const QJsonObject canvasLabelTextEditor = canvasLabelTextEditorValue.toObject();
            preferences.m_canvasLabelTextEditorOpacity = boundedNumberFromJsonValue(
                canvasLabelTextEditor, QStringLiteral("opacity"), QStringLiteral("canvasLabelTextEditor.opacity"),
                preferences.m_canvasLabelTextEditorOpacity, 0.0, 1.0,
                AppPreferenceWarningType::CanvasLabelTextEditorOpacityWrongType,
                AppPreferenceWarningType::CanvasLabelTextEditorOpacityOutOfRange, warnings);
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
                    preferences.m_moveLabelModifiers = modifiersFromString(
                        moveLabelModifierValue.toString(), QStringLiteral("input.moveLabelModifier"),
                        preferences.m_moveLabelModifiers, AppPreferenceWarningType::MoveLabelModifierInvalid, warnings);
                }
                else {
                    warnings.append(makeWarning(AppPreferenceWarningType::MoveLabelModifierInvalid,
                                                QStringLiteral("input.moveLabelModifier")));
                }
            }

            const QJsonObject input = inputValue.toObject();
            const QJsonValue previousLabelModifierValue = input.value(QStringLiteral("previousLabelModifier"));
            if (!previousLabelModifierValue.isUndefined()) {
                if (previousLabelModifierValue.isString()) {
                    preferences.m_previousLabelModifiers = modifiersFromString(
                        previousLabelModifierValue.toString(), QStringLiteral("input.previousLabelModifier"),
                        preferences.m_previousLabelModifiers, AppPreferenceWarningType::PreviousLabelModifierInvalid,
                        warnings);
                }
                else {
                    warnings.append(makeWarning(AppPreferenceWarningType::PreviousLabelModifierInvalid,
                                                QStringLiteral("input.previousLabelModifier")));
                }
            }

            preferences.m_undoShortcut = keySequenceFromJsonValue(
                input.value(QStringLiteral("undoShortcut")), QStringLiteral("input.undoShortcut"),
                preferences.m_undoShortcut, AppPreferenceWarningType::UndoShortcutInvalid, warnings);
            preferences.m_redoShortcut = keySequenceFromJsonValue(
                input.value(QStringLiteral("redoShortcut")), QStringLiteral("input.redoShortcut"),
                preferences.m_redoShortcut, AppPreferenceWarningType::RedoShortcutInvalid, warnings);
            preferences.m_nextLabelShortcut = keySequenceFromJsonValue(
                input.value(QStringLiteral("nextLabelShortcut")), QStringLiteral("input.nextLabelShortcut"),
                preferences.m_nextLabelShortcut, AppPreferenceWarningType::NextLabelShortcutInvalid, warnings);
            preferences.m_alternatePreviousLabelShortcut = keySequenceFromJsonValue(
                input.value(QStringLiteral("alternatePreviousLabelShortcut")),
                QStringLiteral("input.alternatePreviousLabelShortcut"), preferences.m_alternatePreviousLabelShortcut,
                AppPreferenceWarningType::AlternatePreviousLabelShortcutInvalid, warnings);
            preferences.m_alternateNextLabelShortcut = keySequenceFromJsonValue(
                input.value(QStringLiteral("alternateNextLabelShortcut")),
                QStringLiteral("input.alternateNextLabelShortcut"), preferences.m_alternateNextLabelShortcut,
                AppPreferenceWarningType::AlternateNextLabelShortcutInvalid, warnings);
            preferences.m_previousPageShortcut = keySequenceFromJsonValue(
                input.value(QStringLiteral("previousPageShortcut")), QStringLiteral("input.previousPageShortcut"),
                preferences.m_previousPageShortcut, AppPreferenceWarningType::PreviousPageShortcutInvalid, warnings);
            preferences.m_nextPageShortcut = keySequenceFromJsonValue(
                input.value(QStringLiteral("nextPageShortcut")), QStringLiteral("input.nextPageShortcut"),
                preferences.m_nextPageShortcut, AppPreferenceWarningType::NextPageShortcutInvalid, warnings);
            preferences.m_editLabelTextShortcut = keySequenceFromJsonValue(
                input.value(QStringLiteral("editLabelTextShortcut")), QStringLiteral("input.editLabelTextShortcut"),
                preferences.m_editLabelTextShortcut, AppPreferenceWarningType::EditLabelTextShortcutInvalid, warnings);
            preferences.m_commitLabelTextShortcut = keySequenceFromJsonValue(
                input.value(QStringLiteral("commitLabelTextShortcut")), QStringLiteral("input.commitLabelTextShortcut"),
                preferences.m_commitLabelTextShortcut, AppPreferenceWarningType::CommitLabelTextShortcutInvalid,
                warnings);
        }
    }

    const QJsonValue backupPathValue = root.value(QStringLiteral("backupPath"));
    if (!backupPathValue.isUndefined()) {
        if (backupPathValue.isString()) {
            const QString backupPath = backupPathValue.toString().trimmed();
            if (!backupPath.isEmpty()) {
                preferences.m_backupPath = backupPath;
            }
            else {
                warnings.append(
                    makeWarning(AppPreferenceWarningType::BackupPathWrongType, QStringLiteral("backupPath")));
            }
        }
        else {
            warnings.append(makeWarning(AppPreferenceWarningType::BackupPathWrongType, QStringLiteral("backupPath")));
        }
    }

    preferences.m_backupIntervalSeconds = positiveIntegerFromJsonValue(
        root, QStringLiteral("backupIntervalSeconds"), QStringLiteral("backupIntervalSeconds"),
        preferences.m_backupIntervalSeconds, 1, 86400, AppPreferenceWarningType::BackupIntervalWrongType,
        AppPreferenceWarningType::BackupIntervalOutOfRange, warnings);

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

QString AppPreferences::defaultFilePath()
{
    return preferencePath();
}

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
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        AppPreferences preferences;
        QVector<AppPreferenceWarning> warnings;
        warnings.append(makeWarning(AppPreferenceWarningType::FileNotReadable));
        return {preferences, warnings};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    return loadFromDocument(document, &parseError);
}

AppPreferencesLoadResult AppPreferences::loadFromJson(const QByteArray& json)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    return loadFromDocument(document, &parseError);
}

QJsonDocument AppPreferences::toJsonDocument() const
{
    QJsonObject appearance;
    appearance.insert(QStringLiteral("style"), m_applicationStyle);
    appearance.insert(QStringLiteral("theme"), m_applicationTheme);

    QJsonObject labelMarker;
    labelMarker.insert(QStringLiteral("diameter"), m_labelMarkerDiameterPixels);
    labelMarker.insert(QStringLiteral("fontPointSize"), m_labelMarkerFontPointSize);

    QJsonObject labelTable;
    labelTable.insert(QStringLiteral("fontFamily"), m_labelTableFontFamily);
    labelTable.insert(QStringLiteral("fontPointSize"), m_labelTableFontPointSize);
    labelTable.insert(QStringLiteral("maxTextRows"), m_labelTableMaxTextRows);

    QJsonObject labelTextEditor;
    labelTextEditor.insert(QStringLiteral("fontFamily"), m_labelTextEditorFontFamily);
    labelTextEditor.insert(QStringLiteral("fontPointSize"), m_labelTextEditorFontPointSize);

    QJsonObject markerTextBubble;
    markerTextBubble.insert(QStringLiteral("fontFamily"), m_markerTextBubbleFontFamily);
    markerTextBubble.insert(QStringLiteral("fontPointSize"), m_markerTextBubbleFontPointSize);
    markerTextBubble.insert(QStringLiteral("opacity"), m_markerTextBubbleOpacity);

    QJsonObject canvasLabelTextEditor;
    canvasLabelTextEditor.insert(QStringLiteral("opacity"), m_canvasLabelTextEditorOpacity);

    QJsonObject input;
    input.insert(QStringLiteral("moveLabelModifier"), modifiersToString(m_moveLabelModifiers));
    input.insert(QStringLiteral("previousLabelModifier"), modifiersToString(m_previousLabelModifiers));
    input.insert(QStringLiteral("undoShortcut"), m_undoShortcut.toString(QKeySequence::PortableText));
    input.insert(QStringLiteral("redoShortcut"), m_redoShortcut.toString(QKeySequence::PortableText));
    input.insert(QStringLiteral("nextLabelShortcut"), m_nextLabelShortcut.toString(QKeySequence::PortableText));
    input.insert(QStringLiteral("alternatePreviousLabelShortcut"),
                 m_alternatePreviousLabelShortcut.toString(QKeySequence::PortableText));
    input.insert(QStringLiteral("alternateNextLabelShortcut"),
                 m_alternateNextLabelShortcut.toString(QKeySequence::PortableText));
    input.insert(QStringLiteral("previousPageShortcut"), m_previousPageShortcut.toString(QKeySequence::PortableText));
    input.insert(QStringLiteral("nextPageShortcut"), m_nextPageShortcut.toString(QKeySequence::PortableText));
    input.insert(QStringLiteral("editLabelTextShortcut"), m_editLabelTextShortcut.toString(QKeySequence::PortableText));
    input.insert(QStringLiteral("commitLabelTextShortcut"),
                 m_commitLabelTextShortcut.toString(QKeySequence::PortableText));

    QJsonArray groupStyles;
    for (const LabelGroupStyle& groupStyle : m_groupStyles) {
        QJsonObject style;
        style.insert(QStringLiteral("groupColor"),
                     groupStyle.groupColor.isValid() ? groupStyle.groupColor.name() : QString());
        style.insert(QStringLiteral("markerDiameter"), groupStyle.markerDiameter);
        style.insert(QStringLiteral("fontPointSize"), groupStyle.fontPointSize);
        style.insert(QStringLiteral("markerStyle"), markerShapeToString(groupStyle.markerShape));
        groupStyles.append(style);
    }

    QJsonObject root;
    root.insert(QStringLiteral("appearance"), appearance);
    root.insert(QStringLiteral("backupIntervalSeconds"), m_backupIntervalSeconds);
    root.insert(QStringLiteral("backupPath"), m_backupPath);
    root.insert(QStringLiteral("canvasLabelTextEditor"), canvasLabelTextEditor);
    root.insert(QStringLiteral("groupStyles"), groupStyles);
    root.insert(QStringLiteral("input"), input);
    root.insert(QStringLiteral("labelMarker"), labelMarker);
    root.insert(QStringLiteral("labelTable"), labelTable);
    root.insert(QStringLiteral("labelTextEditor"), labelTextEditor);
    root.insert(QStringLiteral("markerTextBubble"), markerTextBubble);
    return QJsonDocument(root);
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

QString AppPreferences::labelTableFontFamily() const
{
    return m_labelTableFontFamily;
}

double AppPreferences::labelTableFontPointSize() const noexcept
{
    return m_labelTableFontPointSize;
}

QString AppPreferences::labelTextEditorFontFamily() const
{
    return m_labelTextEditorFontFamily;
}

double AppPreferences::labelTextEditorFontPointSize() const noexcept
{
    return m_labelTextEditorFontPointSize;
}

QString AppPreferences::markerTextBubbleFontFamily() const
{
    return m_markerTextBubbleFontFamily;
}

double AppPreferences::markerTextBubbleFontPointSize() const noexcept
{
    return m_markerTextBubbleFontPointSize;
}

double AppPreferences::markerTextBubbleOpacity() const noexcept
{
    return m_markerTextBubbleOpacity;
}

double AppPreferences::canvasLabelTextEditorOpacity() const noexcept
{
    return m_canvasLabelTextEditorOpacity;
}

QString AppPreferences::applicationStyle() const
{
    return m_applicationStyle;
}

QString AppPreferences::applicationTheme() const
{
    return m_applicationTheme;
}

Qt::KeyboardModifiers AppPreferences::moveLabelModifiers() const noexcept
{
    return m_moveLabelModifiers;
}

Qt::KeyboardModifiers AppPreferences::previousLabelModifiers() const noexcept
{
    return m_previousLabelModifiers;
}

QKeySequence AppPreferences::undoShortcut() const
{
    return m_undoShortcut;
}

QKeySequence AppPreferences::redoShortcut() const
{
    return m_redoShortcut;
}

QKeySequence AppPreferences::nextLabelShortcut() const
{
    return m_nextLabelShortcut;
}

QKeySequence AppPreferences::alternatePreviousLabelShortcut() const
{
    return m_alternatePreviousLabelShortcut;
}

QKeySequence AppPreferences::alternateNextLabelShortcut() const
{
    return m_alternateNextLabelShortcut;
}

QKeySequence AppPreferences::previousPageShortcut() const
{
    return m_previousPageShortcut;
}

QKeySequence AppPreferences::nextPageShortcut() const
{
    return m_nextPageShortcut;
}

QKeySequence AppPreferences::editLabelTextShortcut() const
{
    return m_editLabelTextShortcut;
}

QKeySequence AppPreferences::commitLabelTextShortcut() const
{
    return m_commitLabelTextShortcut;
}

QString AppPreferences::backupPath() const
{
    return m_backupPath;
}

int AppPreferences::backupIntervalSeconds() const noexcept
{
    return m_backupIntervalSeconds;
}

const QVector<LabelGroupStyle>& AppPreferences::groupStyles() const noexcept
{
    return m_groupStyles;
}

} // namespace labelminus::core
