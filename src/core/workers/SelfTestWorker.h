#pragma once

#include <QObject>
#include <QString>

class SelfTestWorker : public QObject
{
    Q_OBJECT
public slots:
    void runWorkItem(int id, const QString &payload);

signals:
    void workFinished(int id, const QString &payload, const QString &threadName);
};
