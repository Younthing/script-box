#pragma once

#include "common/Dto.h"

#include <QObject>
#include <QString>

class ScanWorker : public QObject
{
    Q_OBJECT
public slots:
    void scan(const QString &toolsRoot);

signals:
    void scanFinished(const ScanResultDTO &result);

private:
    ToolDTO parseTool(const QString &toolDirPath, QString &error) const;
};
