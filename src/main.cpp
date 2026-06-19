#include "ui/MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("LabelMinus");
    QApplication::setOrganizationName("LabelMinus");

    MainWindow window;
    window.show();

    return QApplication::exec();
}

