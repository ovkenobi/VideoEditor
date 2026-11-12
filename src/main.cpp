#include <QApplication>

#include "ui/mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("OC"));
    QApplication::setOrganizationDomain(QStringLiteral("oc.local"));
    QApplication::setApplicationName(QStringLiteral("VideoEditor"));
    QApplication::setApplicationVersion(QStringLiteral("1.0"));

    MainWindow window;
    window.show();

    return app.exec();
}
