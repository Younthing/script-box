#pragma once

#include "common/Dto.h"

#include <QObject>
#include <QString>

class EnvWorker : public QObject
{
    Q_OBJECT
public slots:
    void prepareEnv(const QString &toolsRoot, const ToolDTO &tool);

signals:
    void envReady(const QString &toolId, const QString &envPath);
    void envError(const QString &toolId, const QString &message);

private:
    bool ensurePythonEnv(const QString &toolDir, const ToolDTO &tool, QString &envPath, QString &message) const;
    bool ensureREnv(const QString &toolDir, const ToolDTO &tool, QString &envPath, QString &message) const;
};
