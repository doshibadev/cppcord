#include <QApplication>
#include <QFontDatabase>
#include <QDebug>
#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Set application info for QSettings (used by TokenStorage)
    QCoreApplication::setOrganizationName("CPPCord");
    QCoreApplication::setApplicationName("DiscordClient");

    // Load GG Sans font family
    QStringList fontFiles = {
        ":/fonts/fonts/ggsans.ttf",
        ":/fonts/fonts/ggsansbold.ttf",
        ":/fonts/fonts/ggsansmedium.ttf",
        ":/fonts/fonts/ggsanssemibold.ttf"};

    for (const QString &fontFile : fontFiles)
    {
        int fontId = QFontDatabase::addApplicationFont(fontFile);
        if (fontId == -1)
        {
            qWarning() << "Failed to load font:" << fontFile;
        }
    }

    // Set GG Sans as the default application font
    QFont appFont("gg sans", 10);
    app.setFont(appFont);

    MainWindow window;
    window.show();

    return app.exec();
}
