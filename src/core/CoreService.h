#pragma once

#include "common/Dto.h"

#include <QObject>
#include <QThread>
#include <QStringList>

class SelfTestWorker;
class ScanWorker;
class JobWorker;

class CoreService : public QObject
{
    Q_OBJECT
public:
    explicit CoreService(QObject *parent = nullptr);
    ~CoreService() override;

    void start();
    void shutdown();

    void runSchedulingSelfTest(int taskCount = 3);

    void startScan(const QString &toolsRoot);
    void runJob(const QString &toolsRoot, const ToolDTO &tool, const RunRequestDTO &request);

signals:
    void selfTestProgress(int finished, int total, const QString &threadName);
    void selfTestCompleted(bool success, const QStringList &threadNames);
    void scanFinished(const ScanResultDTO &result);
    void jobStarted(const QString &toolId, const QString &runDirectory);
    void jobOutput(const QString &toolId, const QString &line, bool isError);
    void jobFinished(const QString &toolId, int exitCode, const QString &message);

private slots:
    void handleWorkFinished(int id, const QString &payload, const QString &threadName);
    void handleScanFinished(const ScanResultDTO &result);
    void handleJobStarted(const QString &toolId, const QString &runDirectory);
    void handleJobOutput(const QString &toolId, const QString &line, bool isError);
    void handleJobFinished(const QString &toolId, int exitCode, const QString &message);

private:
    void ensureWorkerReady();
    void ensureScanWorkerReady();
    void ensureJobWorkerReady();

    QThread m_workerThread;
    SelfTestWorker *m_worker{nullptr};

    QThread m_scanThread;
    ScanWorker *m_scanWorker{nullptr};

    QThread m_jobThread;
    JobWorker *m_jobWorker{nullptr};

    int m_expectedTasks{0};
    QStringList m_completedThreadNames;
};
