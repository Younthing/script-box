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
    static QString readScalar(const QStringList &lines, const QString &key);
    static QStringList readList(const QStringList &lines, const QString &key);
    QList<ParamDTO> readParams(const QStringList &lines) const;
};
