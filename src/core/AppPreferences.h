#pragma once

#include <QColor>
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
    GroupStylesNotArray,
    GroupStyleNotObject,
    InvalidGroupStyleColor,
    GroupStyleMarkerSizeWrongType,
    GroupStyleMarkerSizeOutOfRange,
    GroupStyleMarkerStyleInvalid,
    InputNotObject,
    MoveLabelModifierInvalid,
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
    Qt::KeyboardModifiers moveLabelModifiers() const noexcept;
    const QVector<LabelGroupStyle>& groupStyles() const noexcept;

private:
    static AppPreferencesLoadResult loadFromDocument(const QJsonDocument& document, const QJsonParseError* parseError);

    double m_labelMarkerDiameterPixels{20.0};
    double m_labelMarkerFontPointSize{10.0};
    int m_labelTableMaxTextRows{3};
    Qt::KeyboardModifiers m_moveLabelModifiers{Qt::ControlModifier};
    QVector<LabelGroupStyle> m_groupStyles;
};

struct AppPreferencesLoadResult {
    AppPreferences preferences;
    QVector<AppPreferenceWarning> warnings;
};

} // namespace labelminus::core
