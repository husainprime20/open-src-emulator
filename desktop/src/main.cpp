#include "mainwindow.h"

#include <QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("OpenSrc");
    QCoreApplication::setOrganizationDomain("opensrc.local");
    QCoreApplication::setApplicationName("OpenSrc");
    QCoreApplication::setApplicationVersion("0.2.0");
    if (QStyle *fusion = QStyleFactory::create("Fusion"))
        app.setStyle(fusion);

    MainWindow window;
    window.show();
    return app.exec();
}
