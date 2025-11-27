#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

class LoggingBridge : public QObject
{
    Q_OBJECT
public:
    static LoggingBridge *instance();

signals:
    void logMessage(int level, const QString &category, const QString &message);

private:
    explicit LoggingBridge(QObject *parent = nullptr);
    static void handler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

    static QPointer<LoggingBridge> s_instance;
    static QtMessageHandler s_previous;
};
