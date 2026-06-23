#pragma once

#include <QString>
#include <QVector>

class QTranslator;

namespace labelminus::core {

struct ApplicationLanguage {
    QString localeName;
    QString displayName;
};

QVector<ApplicationLanguage> availableApplicationLanguages();
bool loadApplicationTranslator(QTranslator& translator, const QString& localeName);

} // namespace labelminus::core
