#include "ui/MainWindow.h"

#include <QApplication>
#include <QDir>
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
    installTranslator(app);

    MainWindow window;
    window.show();

    return QApplication::exec();
}
