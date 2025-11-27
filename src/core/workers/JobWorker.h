#pragma once

#include "common/Dto.h"

#include <QObject>
#include <QProcess>
#include <QScopedPointer>
#include <QString>

class JobWorker : public QObject
{
    Q_OBJECT
public slots:
    void runJob(const QString &toolsRoot, const ToolDTO &tool, const RunRequestDTO &request, const QString &envPath);
    void cancel();

signals:
    void jobStarted(const QString &toolId, const QString &runDirectory);
    void jobOutput(const QString &toolId, const QString &line, bool isError);
    void jobFinished(const QString &toolId, int exitCode, const QString &message);

private:
    void appendArguments(QProcess &process, const ToolDTO &tool, const RunRequestDTO &request, const QString &envPath) const;
    QString ensureRunDirectory(const QString &toolsRoot, const ToolDTO &tool, const RunRequestDTO &request) const;
    void wireProcessSignals(QProcess &process, const QString &toolId, const QString &runDir);

    QScopedPointer<QProcess> m_process;
};
