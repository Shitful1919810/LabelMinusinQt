#pragma once

#include <QColor>
#include <QString>
#include <QVector>

namespace labelminus::core {

class AppPreferences {
public:
    static AppPreferences load();

    int labelMarkerDiameter() const noexcept;
    int labelMarkerFontPointSize() const noexcept;
    const QVector<QColor>& groupColors() const noexcept;

private:
    int m_labelMarkerDiameter{36};
    int m_labelMarkerFontPointSize{10};
    QVector<QColor> m_groupColors;
};

} // namespace labelminus::core
