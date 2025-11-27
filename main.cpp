#include "core/CoreService.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QDir>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    CoreService core;
    core.start();

    QDir appDir(QCoreApplication::applicationDirPath());
    appDir.cdUp(); // from build/ to repo root
    const QString toolsRoot = appDir.filePath(QStringLiteral("tools"));

    MainWindow window(&core, toolsRoot);
    window.resize(960, 640);
    window.setWindowTitle(QStringLiteral("Script Toolbox"));
    window.show();

    // Kick off a simple scheduling self-test to verify background thread wiring.
    core.runSchedulingSelfTest(3);

    return app.exec();
}
