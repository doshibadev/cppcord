#include <QApplication>
#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Set application info for QSettings (used by TokenStorage)
    QCoreApplication::setOrganizationName("CPPCord");
    QCoreApplication::setApplicationName("DiscordClient");

    MainWindow window;
    window.show();

    return app.exec();
}
