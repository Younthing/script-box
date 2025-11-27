#pragma once

#include "common/Dto.h"

#include <QObject>
#include <QThread>
#include <QStringList>
#include <QHash>

class SelfTestWorker;
class ScanWorker;
class JobWorker;
class EnvWorker;

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
    void runTool(const QString &toolsRoot, const ToolDTO &tool, const RunRequestDTO &request);

signals:
    void selfTestProgress(int finished, int total, const QString &threadName);
    void selfTestCompleted(bool success, const QStringList &threadNames);
    void scanFinished(const ScanResultDTO &result);
    void jobStarted(const QString &toolId, const QString &runDirectory);
    void jobOutput(const QString &toolId, const QString &line, bool isError);
    void jobFinished(const QString &toolId, int exitCode, const QString &message);
    void envPreparing(const QString &toolId);
    void envFailed(const QString &toolId, const QString &message);
    void envReady(const QString &toolId, const QString &envPath);

private slots:
    void handleWorkFinished(int id, const QString &payload, const QString &threadName);
    void handleScanFinished(const ScanResultDTO &result);
    void handleJobStarted(const QString &toolId, const QString &runDirectory);
    void handleJobOutput(const QString &toolId, const QString &line, bool isError);
    void handleJobFinished(const QString &toolId, int exitCode, const QString &message);
    void handleEnvReady(const QString &toolId, const QString &envPath);
    void handleEnvError(const QString &toolId, const QString &message);

private:
    void ensureWorkerReady();
    void ensureScanWorkerReady();
    void ensureJobWorkerReady();
    void ensureEnvWorkerReady();

    QThread m_workerThread;
    SelfTestWorker *m_worker{nullptr};

    QThread m_scanThread;
    ScanWorker *m_scanWorker{nullptr};

    QThread m_jobThread;
    JobWorker *m_jobWorker{nullptr};

    QThread m_envThread;
    EnvWorker *m_envWorker{nullptr};

    int m_expectedTasks{0};
    QStringList m_completedThreadNames;

    struct PendingJob
    {
        QString toolsRoot;
        ToolDTO tool;
        RunRequestDTO request;
    };
    QHash<QString, PendingJob> m_pendingJobs;
};
