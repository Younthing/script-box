#include "core/CoreService.h"

#include <QApplication>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    CoreService core;
    core.start();

    QWidget window;
    window.setWindowTitle("Script Toolbox");
    window.resize(420, 240);

    auto *layout = new QVBoxLayout(&window);
    auto *label = new QLabel("CoreService self-test running...");
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);

    QObject::connect(&core, &CoreService::selfTestProgress, &window, [label](int finished, int total, const QString &threadName) {
        label->setText(QStringLiteral("Self-test progress: %1/%2 (last on %3)")
                           .arg(finished)
                           .arg(total)
                           .arg(threadName));
    });

    QObject::connect(&core, &CoreService::selfTestCompleted, &window, [label](bool ok, const QStringList &threads) {
        QString summary = ok ? QStringLiteral("Self-test passed\nThreads used:\n") : QStringLiteral("Self-test failed\nThreads:\n");
        for (const QString &t : threads)
        {
            summary.append(" - " + t + "\n");
        }
        label->setText(summary.trimmed());
    });

    window.show();

    // Kick off a simple scheduling self-test to verify background thread wiring.
    core.runSchedulingSelfTest(3);

    return app.exec();
}
