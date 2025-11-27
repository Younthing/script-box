#include "core/CoreService.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QDir>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("ScriptToolbox"));
    QCoreApplication::setApplicationName(QStringLiteral("ScriptToolbox"));
    // Version is injected at build time via APP_VERSION (CMake -DAPP_VERSION).
#ifndef APP_VERSION
#define APP_VERSION "0.0.0-dev"
#endif
    QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION));

    CoreService core;
    core.start();

    QDir exeDir(QCoreApplication::applicationDirPath());
    QStringList candidates;
    candidates << QDir(exeDir).filePath(QStringLiteral("tools"));
    QDir parentDir(exeDir);
    parentDir.cdUp();
    candidates << parentDir.filePath(QStringLiteral("tools"));

    QString toolsRoot;
    for (const QString &c : candidates)
    {
        if (QDir(c).exists())
        {
            toolsRoot = c;
            break;
        }
    }
    if (toolsRoot.isEmpty())
    {
        toolsRoot = candidates.value(0);
    }

    MainWindow window(&core, toolsRoot);
    window.resize(960, 640);
    window.setWindowTitle(QStringLiteral("Script Toolbox"));
    window.show();

    // Kick off a simple scheduling self-test to verify background thread wiring.
    core.runSchedulingSelfTest(3);

    return app.exec();
}
