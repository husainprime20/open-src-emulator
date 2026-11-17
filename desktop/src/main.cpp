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

    // The Windows installer launches OpenSrc with --setup-runtime right after
    // installation, so a fresh install goes straight to Android setup.
    MainWindow window;
    window.show();
    if (app.arguments().contains("--setup-runtime"))
        window.openAndroidSetup();
    return app.exec();
}
