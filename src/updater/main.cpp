#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTextStream>
#include <QThread>
#include <QStringList>

#include <windows.h>

namespace
{
    QFile s_logFile;

    void logLine(const QString &msg)
    {
        if (!s_logFile.isOpen())
        {
            return;
        }
        QTextStream ts(&s_logFile);
        ts.setEncoding(QStringConverter::Utf8);
        ts << QDateTime::currentDateTime().toString(Qt::ISODate) << '\t' << msg << '\n';
        ts.flush();
    }

    bool waitForPidExit(qint64 pid, int timeoutMs = 0)
    {
        const qint64 sleepMs = 500;
        qint64 waited = 0;
        while (true)
        {
            HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
            if (h)
            {
                DWORD code = 0;
                if (GetExitCodeProcess(h, &code) && code != STILL_ACTIVE)
                {
                    CloseHandle(h);
                    return true;
                }
                CloseHandle(h);
            }
            else
            {
                // Cannot open process; assume exited.
                return true;
            }
            if (timeoutMs > 0 && waited >= timeoutMs)
            {
                return false;
            }
            QThread::msleep(sleepMs);
            waited += sleepMs;
        }
    }

    bool runExpandArchive(const QString &zip, const QString &dest)
    {
        auto esc = [](const QString &s)
        {
            QString tmp = s;
            tmp.replace("'", "''");
            return QStringLiteral("'%1'").arg(tmp);
        };

        QStringList args;
        args << QStringLiteral("-NoProfile") << QStringLiteral("-ExecutionPolicy") << QStringLiteral("Bypass")
             << QStringLiteral("-Command")
             << QStringLiteral("Expand-Archive -Path %1 -DestinationPath %2 -Force")
                    .arg(esc(zip), esc(dest));
        const int code = QProcess::execute(QStringLiteral("powershell"), args);
        return code == 0;
    }

    bool copyRecursive(const QString &srcRoot, const QString &dstRoot)
    {
        QDir dstDir(dstRoot);
        if (!dstDir.exists() && !dstDir.mkpath(QStringLiteral(".")))
        {
            return false;
        }

        QDirIterator it(srcRoot, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext())
        {
            it.next();
            const QFileInfo fi = it.fileInfo();
            const QString relative = QDir(srcRoot).relativeFilePath(fi.filePath());
            const QString targetPath = dstDir.filePath(relative);

            if (fi.isDir())
            {
                QDir tp = QFileInfo(targetPath).dir();
                if (!tp.exists() && !tp.mkpath(QStringLiteral(".")))
                {
                    logLine(QStringLiteral("Failed to create dir: %1").arg(tp.absolutePath()));
                    return false;
                }
                continue;
            }

            QDir targetParent = QFileInfo(targetPath).dir();
            if (!targetParent.exists() && !targetParent.mkpath(QStringLiteral(".")))
            {
                logLine(QStringLiteral("Failed to create dir: %1").arg(targetParent.absolutePath()));
                return false;
            }

            if (QFile::exists(targetPath))
            {
                QFile::remove(targetPath);
            }
            if (!QFile::copy(fi.filePath(), targetPath))
            {
                logLine(QStringLiteral("Copy failed: %1 -> %2").arg(fi.filePath(), targetPath));
                return false;
            }
        }
        return true;
    }
} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QString zipPath;
    QString targetDir;
    QString exeName;
    qint64 pid = 0;
    QString logPath;

    for (int i = 1; i < argc; ++i)
    {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        auto next = [&](QString &out)
        {
            if (i + 1 < argc)
            {
                out = QString::fromLocal8Bit(argv[++i]);
                return true;
            }
            return false;
        };

        if (arg == QStringLiteral("--zip"))
        {
            next(zipPath);
        }
        else if (arg == QStringLiteral("--target"))
        {
            next(targetDir);
        }
        else if (arg == QStringLiteral("--exe"))
        {
            next(exeName);
        }
        else if (arg == QStringLiteral("--pid"))
        {
            QString pidStr;
            if (next(pidStr))
            {
                pid = pidStr.toLongLong();
            }
        }
        else if (arg == QStringLiteral("--log"))
        {
            next(logPath);
        }
    }

    if (zipPath.isEmpty() || targetDir.isEmpty() || exeName.isEmpty() || pid <= 0)
    {
        return 2;
    }

    if (logPath.isEmpty())
    {
        const QString base = QDir::temp().filePath(QStringLiteral("ScriptToolboxUpdate_%1").arg(QDateTime::currentMSecsSinceEpoch()));
        QDir().mkpath(base);
        logPath = QDir(base).filePath(QStringLiteral("update.log"));
    }
    QFileInfo logInfo(logPath);
    QDir().mkpath(logInfo.dir().absolutePath());
    s_logFile.setFileName(logPath);
    s_logFile.open(QIODevice::Append | QIODevice::Text);

    logLine(QStringLiteral("Updater started"));
    logLine(QStringLiteral("Zip: %1").arg(zipPath));
    logLine(QStringLiteral("Target: %1").arg(targetDir));
    logLine(QStringLiteral("Exe: %1").arg(exeName));
    logLine(QStringLiteral("PID: %1").arg(pid));

    if (!waitForPidExit(pid, 60000))
    {
        logLine(QStringLiteral("Timeout waiting for pid %1").arg(pid));
        return 3;
    }

    const QString extractDir = QFileInfo(zipPath).absoluteDir().filePath(QStringLiteral("st_update_unpack"));
    QDir().mkpath(extractDir);

    logLine(QStringLiteral("Expanding archive"));
    if (!runExpandArchive(zipPath, extractDir))
    {
        logLine(QStringLiteral("Expand-Archive failed"));
        return 4;
    }

    logLine(QStringLiteral("Copying files"));
    if (!copyRecursive(extractDir, targetDir))
    {
        logLine(QStringLiteral("Copy failed"));
        return 5;
    }

    logLine(QStringLiteral("Cleanup temp"));
    QDir(extractDir).removeRecursively();
    QFile::remove(zipPath);

    const QString nextExe = QDir(targetDir).filePath(exeName);
    logLine(QStringLiteral("Launching new app: %1").arg(nextExe));
    QProcess::startDetached(nextExe);

    logLine(QStringLiteral("Update completed"));
    return 0;
}
