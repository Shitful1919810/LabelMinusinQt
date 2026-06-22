#include "ui/MainWindow.h"
#include "ui/ThemeManager.h"

#include "core/AppPreferences.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QStyle>
#include <QStyleFactory>
#include <QTranslator>

namespace {
void installTranslator(QApplication& app)
{
    auto* translator = new QTranslator(&app);
    const QLocale locale;
    const QString appDir = QDir(QApplication::applicationDirPath()).filePath(QStringLiteral("i18n"));

    if (translator->load(locale, QStringLiteral("labelminus"), QStringLiteral("_"), QStringLiteral(":/i18n")) ||
        translator->load(locale, QStringLiteral("labelminus"), QStringLiteral("_"), appDir)) {
        app.installTranslator(translator);
        return;
    }

    translator->deleteLater();
}

void applyApplicationStyle(const QString& styleName)
{
    if (!styleName.isEmpty() && QStyleFactory::keys().contains(styleName, Qt::CaseInsensitive)) {
        QApplication::setStyle(styleName);
    }
}
} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setProperty("labelminus.defaultStyle", app.style()->objectName());
    const labelminus::core::AppPreferencesLoadResult preferences =
        labelminus::core::AppPreferences::loadWithDiagnostics();
    applyApplicationStyle(preferences.preferences.applicationStyle());
    labelminus::ui::applyApplicationTheme(preferences.preferences.applicationTheme());
    QApplication::setApplicationName("LabelMinus");
    QApplication::setOrganizationName("LabelMinus");
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    installTranslator(app);

    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("main", "LabelPlus text project editor."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("project"),
                                 QCoreApplication::translate("main", "LabelPlus text project to open."));
    parser.process(app);

    MainWindow window;
    window.show();

    const QStringList positionalArguments = parser.positionalArguments();
    if (!positionalArguments.isEmpty()) {
        window.openProjectFile(QFileInfo(positionalArguments.first()).absoluteFilePath());
    }
    else {
        window.openMostRecentProject();
    }

    return QApplication::exec();
}
