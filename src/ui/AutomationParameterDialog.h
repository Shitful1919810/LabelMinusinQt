#pragma once

#include "core/AppPreferences.h"
#include "services/AutomationService.h"

#include <QJsonObject>
#include <QMap>

#include <optional>

class QWidget;

class AutomationParameterDialog final {
public:
    struct Values {
        QJsonObject parameters;
        QMap<QString, QString> secrets;
    };

    static std::optional<Values> getValues(QWidget* parent, const labelminus::services::AutomationScript& script,
                                           const QStringList& groups,
                                           const QVector<labelminus::core::LabelGroupStyle>& groupStyles);
};
