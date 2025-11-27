#include "LoggingBridge.h"

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QMutex>
#include <QMutexLocker>

QPointer<LoggingBridge> LoggingBridge::s_instance = nullptr;
QtMessageHandler LoggingBridge::s_previous = nullptr;

LoggingBridge *LoggingBridge::instance()
{
    static QMutex mutex;
    QMutexLocker lock(&mutex);
    if (s_instance.isNull())
    {
        s_instance = new LoggingBridge(QCoreApplication::instance());
        s_previous = qInstallMessageHandler(&LoggingBridge::handler);
    }
    return s_instance;
}

LoggingBridge::LoggingBridge(QObject *parent)
    : QObject(parent)
{
}

void LoggingBridge::handler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (!s_instance)
    {
        if (s_previous)
        {
            s_previous(type, context, msg);
        }
        return;
    }
    QString category = context.category ? QString::fromLatin1(context.category) : QStringLiteral("default");
    emit s_instance->logMessage(static_cast<int>(type), category, msg);
    if (s_previous)
    {
        s_previous(type, context, msg);
    }
}
