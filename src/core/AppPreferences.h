#pragma once

#include <QColor>
#include <QKeySequence>
#include <QString>
#include <QVector>
#include <Qt>

class QJsonDocument;
class QJsonParseError;

namespace labelminus::core {

enum class AppPreferenceWarningType {
    FileNotReadable,
    InvalidJson,
    RootNotObject,
    LabelMarkerNotObject,
    MarkerSizeWrongType,
    MarkerSizeOutOfRange,
    LabelTableNotObject,
    LabelTableMaxTextRowsWrongType,
    LabelTableMaxTextRowsOutOfRange,
    LabelTableFontFamilyWrongType,
    LabelTableFontPointSizeWrongType,
    LabelTableFontPointSizeOutOfRange,
    LabelTextEditorNotObject,
    LabelTextEditorFontFamilyWrongType,
    LabelTextEditorFontPointSizeWrongType,
    LabelTextEditorFontPointSizeOutOfRange,
    MarkerTextBubbleNotObject,
    MarkerTextBubbleFontFamilyWrongType,
    MarkerTextBubbleFontPointSizeWrongType,
    MarkerTextBubbleFontPointSizeOutOfRange,
    MarkerTextBubbleOpacityWrongType,
    MarkerTextBubbleOpacityOutOfRange,
    GroupStylesNotArray,
    GroupStyleNotObject,
    InvalidGroupStyleColor,
    GroupStyleMarkerSizeWrongType,
    GroupStyleMarkerSizeOutOfRange,
    GroupStyleMarkerStyleInvalid,
    InputNotObject,
    MoveLabelModifierInvalid,
    PreviousLabelModifierInvalid,
    UndoShortcutInvalid,
    RedoShortcutInvalid,
    NextLabelShortcutInvalid,
    AlternatePreviousLabelShortcutInvalid,
    AlternateNextLabelShortcutInvalid,
    PreviousPageShortcutInvalid,
    NextPageShortcutInvalid,
    EditLabelTextShortcutInvalid,
    CommitLabelTextShortcutInvalid,
    BackupPathWrongType,
    BackupIntervalWrongType,
    BackupIntervalOutOfRange,
    AppearanceNotObject,
    AppearanceStyleWrongType,
    AppearanceThemeWrongType,
};

struct AppPreferenceWarning {
    AppPreferenceWarningType type;
    QString key;
    QString detail;
    qsizetype index{-1};
};

struct AppPreferencesLoadResult;

enum class MarkerShape {
    Circle,
    Square,
};

struct LabelGroupStyle {
    QColor groupColor;
    double markerDiameter{20.0};
    double fontPointSize{10.0};
    MarkerShape markerShape{MarkerShape::Circle};
};

class AppPreferences {
public:
    static QString defaultFilePath();
    static AppPreferences load();
    static AppPreferencesLoadResult loadWithDiagnostics();
    static AppPreferencesLoadResult loadFromFile(const QString& path);
    static AppPreferencesLoadResult loadFromJson(const QByteArray& json);

    double labelMarkerDiameterPixels() const noexcept;
    double labelMarkerFontPointSize() const noexcept;
    int labelTableMaxTextRows() const noexcept;
    QString labelTableFontFamily() const;
    double labelTableFontPointSize() const noexcept;
    QString labelTextEditorFontFamily() const;
    double labelTextEditorFontPointSize() const noexcept;
    QString markerTextBubbleFontFamily() const;
    double markerTextBubbleFontPointSize() const noexcept;
    double markerTextBubbleOpacity() const noexcept;
    QString applicationStyle() const;
    QString applicationTheme() const;
    Qt::KeyboardModifiers moveLabelModifiers() const noexcept;
    Qt::KeyboardModifiers previousLabelModifiers() const noexcept;
    QKeySequence undoShortcut() const;
    QKeySequence redoShortcut() const;
    QKeySequence nextLabelShortcut() const;
    QKeySequence alternatePreviousLabelShortcut() const;
    QKeySequence alternateNextLabelShortcut() const;
    QKeySequence previousPageShortcut() const;
    QKeySequence nextPageShortcut() const;
    QKeySequence editLabelTextShortcut() const;
    QKeySequence commitLabelTextShortcut() const;
    QString backupPath() const;
    int backupIntervalSeconds() const noexcept;
    const QVector<LabelGroupStyle>& groupStyles() const noexcept;

private:
    static AppPreferencesLoadResult loadFromDocument(const QJsonDocument& document, const QJsonParseError* parseError);

    double m_labelMarkerDiameterPixels{20.0};
    double m_labelMarkerFontPointSize{10.0};
    int m_labelTableMaxTextRows{3};
    QString m_labelTableFontFamily;
    double m_labelTableFontPointSize{0.0};
    QString m_labelTextEditorFontFamily;
    double m_labelTextEditorFontPointSize{0.0};
    QString m_markerTextBubbleFontFamily;
    double m_markerTextBubbleFontPointSize{0.0};
    double m_markerTextBubbleOpacity{1.0};
    QString m_applicationStyle;
    QString m_applicationTheme;
    Qt::KeyboardModifiers m_moveLabelModifiers{Qt::ControlModifier};
    Qt::KeyboardModifiers m_previousLabelModifiers{Qt::ControlModifier};
    QKeySequence m_undoShortcut{QStringLiteral("Ctrl+Z")};
    QKeySequence m_redoShortcut{QStringLiteral("Ctrl+Y")};
    QKeySequence m_nextLabelShortcut{QStringLiteral("Tab")};
    QKeySequence m_alternatePreviousLabelShortcut{QStringLiteral("Ctrl+Up")};
    QKeySequence m_alternateNextLabelShortcut{QStringLiteral("Ctrl+Down")};
    QKeySequence m_previousPageShortcut{QStringLiteral("Alt+Left")};
    QKeySequence m_nextPageShortcut{QStringLiteral("Alt+Right")};
    QKeySequence m_editLabelTextShortcut{QStringLiteral("Return")};
    QKeySequence m_commitLabelTextShortcut{QStringLiteral("Ctrl+Return")};
    QString m_backupPath{QStringLiteral("bak")};
    int m_backupIntervalSeconds{60};
    QVector<LabelGroupStyle> m_groupStyles;
};

struct AppPreferencesLoadResult {
    AppPreferences preferences;
    QVector<AppPreferenceWarning> warnings;
};

} // namespace labelminus::core
