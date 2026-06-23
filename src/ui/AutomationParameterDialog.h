#pragma once

#include "core/AppPreferences.h"
#include "services/AutomationService.h"

#include <QJsonObject>

#include <optional>

class QWidget;

class AutomationParameterDialog final {
public:
    static std::optional<QJsonObject> getParameters(QWidget* parent,
                                                    const labelminus::services::AutomationScript& script,
                                                    const QStringList& groups,
                                                    const QVector<labelminus::core::LabelGroupStyle>& groupStyles);
};
