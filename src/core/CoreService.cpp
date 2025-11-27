#include "CoreService.h"

#include "core/workers/SelfTestWorker.h"

#include <QMetaObject>
#include <QThread>

CoreService::CoreService(QObject *parent)
    : QObject(parent)
{
    m_workerThread.setObjectName(QStringLiteral("CoreWorker"));
}

CoreService::~CoreService()
{
    shutdown();
}

void CoreService::start()
{
    ensureWorkerReady();
}

void CoreService::shutdown()
{
    if (m_workerThread.isRunning())
    {
        m_workerThread.quit();
        m_workerThread.wait();
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
