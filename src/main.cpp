#include "ui/MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
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
} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
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

    return QApplication::exec();
}
