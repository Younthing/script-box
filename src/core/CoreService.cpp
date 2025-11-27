#include "CoreService.h"

#include "core/workers/JobWorker.h"
#include "core/workers/ScanWorker.h"
#include "core/workers/EnvWorker.h"
#include "core/workers/SelfTestWorker.h"
#include "core/LoggingBridge.h"

#include <QMetaObject>
#include <QMetaType>
#include <QThread>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logCore, "core.service")

CoreService::CoreService(QObject *parent)
    : QObject(parent)
{
    m_workerThread.setObjectName(QStringLiteral("CoreWorker"));
    m_scanThread.setObjectName(QStringLiteral("ScanWorkerThread"));
    m_jobThread.setObjectName(QStringLiteral("JobWorkerThread"));
    m_envThread.setObjectName(QStringLiteral("EnvWorkerThread"));

    qRegisterMetaType<ScanResultDTO>("ScanResultDTO");
    qRegisterMetaType<ToolDTO>("ToolDTO");
    qRegisterMetaType<RunRequestDTO>("RunRequestDTO");
    qRegisterMetaType<RunParamValueDTO>("RunParamValueDTO");

    LoggingBridge::instance();
}

CoreService::~CoreService()
{
    shutdown();
}

void CoreService::start()
{
    ensureWorkerReady();
    qInfo(logCore) << "CoreService started";
}

void CoreService::shutdown()
{
    qInfo(logCore) << "CoreService shutting down";
    if (m_workerThread.isRunning())
    {
        m_workerThread.quit();
        m_workerThread.wait();
    }

    if (m_scanThread.isRunning())
    {
        m_scanThread.quit();
        m_scanThread.wait();
    }

    if (m_jobThread.isRunning())
    {
        m_jobThread.quit();
        m_jobThread.wait();
    }

    if (m_envThread.isRunning())
    {
        m_envThread.quit();
        m_envThread.wait();
    }
}

void CoreService::runSchedulingSelfTest(int taskCount)
{
    ensureWorkerReady();

    m_expectedTasks = taskCount;
    m_completedThreadNames.clear();

    for (int i = 0; i < taskCount; ++i)
    {
        const QString payload = QStringLiteral("task-%1").arg(i);
        QMetaObject::invokeMethod(
            m_worker,
            "runWorkItem",
            Qt::QueuedConnection,
            Q_ARG(int, i),
            Q_ARG(QString, payload));
    }
}

void CoreService::startScan(const QString &toolsRoot)
{
    ensureScanWorkerReady();
    qInfo(logCore) << "Start scan" << toolsRoot;
    QMetaObject::invokeMethod(m_scanWorker, "scan", Qt::QueuedConnection, Q_ARG(QString, toolsRoot));
}

void CoreService::runJob(const QString &toolsRoot, const ToolDTO &tool, const RunRequestDTO &request)
{
    ensureJobWorkerReady();
    qInfo(logCore) << "Run job directly" << tool.id;
    QMetaObject::invokeMethod(
        m_jobWorker,
        "runJob",
        Qt::QueuedConnection,
        Q_ARG(QString, toolsRoot),
        Q_ARG(ToolDTO, tool),
        Q_ARG(RunRequestDTO, request),
        Q_ARG(QString, QString()));
}

void CoreService::runTool(const QString &toolsRoot, const ToolDTO &tool, const RunRequestDTO &request)
{
    ensureEnvWorkerReady();
    ensureJobWorkerReady();

    PendingJob pending{toolsRoot, tool, request};
    m_pendingJobs.insert(tool.id, pending);

    emit envPreparing(tool.id);
    qInfo(logCore) << "Prepare env then run" << tool.id;
    QMetaObject::invokeMethod(
        m_envWorker,
        "prepareEnv",
        Qt::QueuedConnection,
        Q_ARG(QString, toolsRoot),
        Q_ARG(ToolDTO, tool));
}

void CoreService::handleWorkFinished(int id, const QString &payload, const QString &threadName)
{
    Q_UNUSED(id);
    Q_UNUSED(payload);

    m_completedThreadNames << threadName;
    emit selfTestProgress(m_completedThreadNames.size(), m_expectedTasks, threadName);

    if (m_completedThreadNames.size() == m_expectedTasks)
    {
        emit selfTestCompleted(true, m_completedThreadNames);
    }
}

void CoreService::handleScanFinished(const ScanResultDTO &result)
{
    emit scanFinished(result);
}

void CoreService::handleJobStarted(const QString &toolId, const QString &runDirectory)
{
    emit jobStarted(toolId, runDirectory);
}

void CoreService::handleJobOutput(const QString &toolId, const QString &line, bool isError)
{
    emit jobOutput(toolId, line, isError);
}

void CoreService::handleJobFinished(const QString &toolId, int exitCode, const QString &message)
{
    emit jobFinished(toolId, exitCode, message);
}

void CoreService::handleEnvReady(const QString &toolId, const QString &envPath)
{
    emit envReady(toolId, envPath);

    if (!m_pendingJobs.contains(toolId))
    {
        return;
    }
    auto pending = m_pendingJobs.take(toolId);
    QMetaObject::invokeMethod(
        m_jobWorker,
        "runJob",
        Qt::QueuedConnection,
        Q_ARG(QString, pending.toolsRoot),
        Q_ARG(ToolDTO, pending.tool),
        Q_ARG(RunRequestDTO, pending.request),
        Q_ARG(QString, envPath));
}

void CoreService::handleEnvError(const QString &toolId, const QString &message)
{
    emit envFailed(toolId, message);
    m_pendingJobs.remove(toolId);
}

void CoreService::ensureWorkerReady()
{
    if (!m_worker)
    {
        m_worker = new SelfTestWorker();
        m_worker->moveToThread(&m_workerThread);

        connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
        connect(m_worker, &SelfTestWorker::workFinished, this, &CoreService::handleWorkFinished);
    }

    if (!m_workerThread.isRunning())
    {
        m_workerThread.start();
    }
}

void CoreService::ensureScanWorkerReady()
{
    if (!m_scanWorker)
    {
        m_scanWorker = new ScanWorker();
        m_scanWorker->moveToThread(&m_scanThread);

        connect(&m_scanThread, &QThread::finished, m_scanWorker, &QObject::deleteLater);
        connect(m_scanWorker, &ScanWorker::scanFinished, this, &CoreService::handleScanFinished);
    }

    if (!m_scanThread.isRunning())
    {
        m_scanThread.start();
    }
}

void CoreService::ensureJobWorkerReady()
{
    if (!m_jobWorker)
    {
        m_jobWorker = new JobWorker();
        m_jobWorker->moveToThread(&m_jobThread);

        connect(&m_jobThread, &QThread::finished, m_jobWorker, &QObject::deleteLater);
        connect(m_jobWorker, &JobWorker::jobStarted, this, &CoreService::handleJobStarted);
        connect(m_jobWorker, &JobWorker::jobOutput, this, &CoreService::handleJobOutput);
        connect(m_jobWorker, &JobWorker::jobFinished, this, &CoreService::handleJobFinished);
    }

    if (!m_jobThread.isRunning())
    {
        m_jobThread.start();
    }
}

void CoreService::ensureEnvWorkerReady()
{
    if (!m_envWorker)
    {
        m_envWorker = new EnvWorker();
        m_envWorker->moveToThread(&m_envThread);

        connect(&m_envThread, &QThread::finished, m_envWorker, &QObject::deleteLater);
        connect(m_envWorker, &EnvWorker::envReady, this, &CoreService::handleEnvReady);
        connect(m_envWorker, &EnvWorker::envError, this, &CoreService::handleEnvError);
    }

    if (!m_envThread.isRunning())
    {
        m_envThread.start();
    }
}
