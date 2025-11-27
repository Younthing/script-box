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
    bool prepareByStrategy(const QString &toolDir, const ToolDTO &tool, QString &envPath, QString &message) const;
    bool ensureUvEnv(const QString &toolDir, const ToolDTO &tool, QString &envPath, QString &message) const;
    bool ensurePakEnv(const QString &toolDir, const ToolDTO &tool, QString &envPath, QString &message) const;
    bool runSetupCommand(const QString &toolDir, const SetupCommandDTO &setup, QString &message) const;
};
