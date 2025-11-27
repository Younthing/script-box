#pragma once

#include "common/Dto.h"

#include <QObject>
#include <QThread>
#include <QStringList>

class SelfTestWorker;

class CoreService : public QObject
{
    Q_OBJECT
public:
    explicit CoreService(QObject *parent = nullptr);
    ~CoreService() override;

    void start();
    void shutdown();

    void runSchedulingSelfTest(int taskCount = 3);

signals:
    void selfTestProgress(int finished, int total, const QString &threadName);
    void selfTestCompleted(bool success, const QStringList &threadNames);

private slots:
    void handleWorkFinished(int id, const QString &payload, const QString &threadName);

private:
    void ensureWorkerReady();

    QThread m_workerThread;
    SelfTestWorker *m_worker{nullptr};

    int m_expectedTasks{0};
    QStringList m_completedThreadNames;
};
