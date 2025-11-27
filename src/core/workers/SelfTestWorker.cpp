#include "SelfTestWorker.h"

#include <QThread>

namespace
{
    QString resolveThreadName()
    {
        QString name = QThread::currentThread()->objectName();
        if (!name.isEmpty())
        {
            return name;
        }

        quintptr id = reinterpret_cast<quintptr>(QThread::currentThreadId());
        return QStringLiteral("Worker-%1").arg(id, 0, 16);
    }
} // namespace

void SelfTestWorker::runWorkItem(int id, const QString &payload)
{
    Q_UNUSED(payload);

    // Simulate lightweight background work.
    QThread::msleep(50 + (id * 10));

    emit workFinished(id, payload, resolveThreadName());
}
