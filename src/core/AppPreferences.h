#pragma once

#include <QColor>
#include <QString>
#include <QVector>

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
    GroupColorsNotArray,
    InvalidGroupColor,
};

struct AppPreferenceWarning {
    AppPreferenceWarningType type;
    QString key;
    QString detail;
    qsizetype index{-1};
};

struct AppPreferencesLoadResult;

class AppPreferences {
public:
    static AppPreferences load();
    static AppPreferencesLoadResult loadWithDiagnostics();
    static AppPreferencesLoadResult loadFromFile(const QString& path);

    double labelMarkerDiameterPixels() const noexcept;
    double labelMarkerFontPointSize() const noexcept;
    int labelTableMaxTextRows() const noexcept;
    const QVector<QColor>& groupColors() const noexcept;

private:
    double m_labelMarkerDiameterPixels{4.0};
    double m_labelMarkerFontPointSize{2.5};
    int m_labelTableMaxTextRows{3};
    QVector<QColor> m_groupColors;
};

struct AppPreferencesLoadResult {
    AppPreferences preferences;
    QVector<AppPreferenceWarning> warnings;
};

} // namespace labelminus::core
