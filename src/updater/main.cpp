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
    QTextStream s_console(stdout);

    void emitMsg(const QString &msg)
    {
        s_console << msg << '\n';
        s_console.flush();
        if (s_logFile.isOpen())
        {
            QTextStream ts(&s_logFile);
            ts.setEncoding(QStringConverter::Utf8);
            ts << QDateTime::currentDateTime().toString(Qt::ISODate) << '\t' << msg << '\n';
            ts.flush();
        }
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
                    emitMsg(QStringLiteral("Failed to create dir: %1").arg(tp.absolutePath()));
                    return false;
                }
                continue;
            }

            QDir targetParent = QFileInfo(targetPath).dir();
            if (!targetParent.exists() && !targetParent.mkpath(QStringLiteral(".")))
            {
                emitMsg(QStringLiteral("Failed to create dir: %1").arg(targetParent.absolutePath()));
                return false;
            }

            if (QFile::exists(targetPath))
            {
                if (!QFile::remove(targetPath))
                {
                    emitMsg(QStringLiteral("Failed to remove existing file: %1").arg(targetPath));
                    return false;
                }
            }
            if (!QFile::copy(fi.filePath(), targetPath))
            {
                emitMsg(QStringLiteral("Copy failed: %1 -> %2").arg(fi.filePath(), targetPath));
                return false;
            }
        }
        return true;
    }

    void scheduleDelayedDelete(const QString &path)
    {
        // Try to delete after this process exits to clean up temp unpack dir when running from it.
        const QString nativePath = QDir::toNativeSeparators(path);
        QStringList args;
        args << QStringLiteral("/C")
             << QStringLiteral("ping -n 3 127.0.0.1 >NUL & rmdir /S /Q \"%1\"").arg(nativePath);
        QProcess::startDetached(QStringLiteral("cmd"), args);
    }
} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    s_console.setEncoding(QStringConverter::Utf8);

    QString zipPath;
    QString targetDir;
    QString exeName;
    qint64 pid = 0;
    QString logPath;
    bool skipExtract = false;

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
        else if (arg == QStringLiteral("--skip-extract"))
        {
            skipExtract = true;
        }
    }

    if (zipPath.isEmpty() || targetDir.isEmpty() || exeName.isEmpty() || pid <= 0)
    {
        emitMsg(QStringLiteral("Usage: updater --zip <path> --target <dir> --exe <name> --pid <processId> [--log <path>] [--skip-extract]"));
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
    if (!s_logFile.open(QIODevice::Append | QIODevice::Text))
    {
        emitMsg(QStringLiteral("Warning: failed to open log file: %1").arg(logPath));
    }

    emitMsg(QStringLiteral("Updater started"));
    emitMsg(QStringLiteral("Log: %1").arg(logPath));
    emitMsg(QStringLiteral("Zip: %1").arg(zipPath));
    emitMsg(QStringLiteral("Target: %1").arg(targetDir));
    emitMsg(QStringLiteral("Exe: %1").arg(exeName));
    emitMsg(QStringLiteral("PID: %1").arg(pid));

    auto canonicalPath = [](const QString &path)
    {
        QFileInfo fi(path);
        QString c = fi.canonicalFilePath();
        if (c.isEmpty())
        {
            c = fi.absoluteFilePath();
        }
        return QDir::cleanPath(c);
    };

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString appCanonical = canonicalPath(appDir);
    const QString targetCanonical = canonicalPath(targetDir);
    const bool runningFromTarget = (!targetCanonical.isEmpty() && appCanonical == targetCanonical);

    if (!waitForPidExit(pid, 60000))
    {
        emitMsg(QStringLiteral("Timeout waiting for pid %1").arg(pid));
        return 3;
    }

    const QString extractDir = QFileInfo(zipPath).absoluteDir().filePath(QStringLiteral("st_update_unpack"));
    const QString extractCanonical = canonicalPath(extractDir);
    const bool runningFromExtract = (!extractCanonical.isEmpty() && appCanonical == extractCanonical);

    if (skipExtract)
    {
        emitMsg(QStringLiteral("Skip extracting archive (handoff mode)"));
        if (!QDir(extractDir).exists())
        {
            emitMsg(QStringLiteral("Extract directory not found: %1").arg(extractDir));
            return 4;
        }
    }
    else
    {
        QDir().mkpath(extractDir);
        emitMsg(QStringLiteral("Expanding archive"));
        if (!runExpandArchive(zipPath, extractDir))
        {
            emitMsg(QStringLiteral("Expand-Archive failed"));
            return 4;
        }
    }

    if (!skipExtract && runningFromTarget)
    {
        const QString stagedUpdater = QDir(extractDir).filePath(QStringLiteral("updater.exe"));
        if (QFileInfo::exists(stagedUpdater))
        {
            QStringList relaunchArgs;
            relaunchArgs << QStringLiteral("--zip") << zipPath;
            relaunchArgs << QStringLiteral("--target") << targetDir;
            relaunchArgs << QStringLiteral("--exe") << exeName;
            relaunchArgs << QStringLiteral("--pid") << QString::number(pid);
            if (!logPath.isEmpty())
            {
                relaunchArgs << QStringLiteral("--log") << logPath;
            }
            relaunchArgs << QStringLiteral("--skip-extract");

            emitMsg(QStringLiteral("Handoff to extracted updater to avoid locked files: %1").arg(stagedUpdater));
            if (QProcess::startDetached(stagedUpdater, relaunchArgs))
            {
                emitMsg(QStringLiteral("Handoff succeeded; exiting current updater."));
                return 0;
            }
            emitMsg(QStringLiteral("Handoff failed; continuing in current updater."));
        }
        else
        {
            emitMsg(QStringLiteral("Extracted updater not found at %1").arg(stagedUpdater));
        }
    }

    emitMsg(QStringLiteral("Copying files"));
    if (!copyRecursive(extractDir, targetDir))
    {
        emitMsg(QStringLiteral("Copy failed"));
        return 5;
    }

    emitMsg(QStringLiteral("Cleanup temp"));
    if (!runningFromExtract)
    {
        QDir(extractDir).removeRecursively();
    }
    else
    {
        emitMsg(QStringLiteral("Skip removing extract dir because updater is running from it."));
        scheduleDelayedDelete(extractDir);
    }
    QFile::remove(zipPath);

    const QString nextExe = QDir(targetDir).filePath(exeName);
    emitMsg(QStringLiteral("Launching new app: %1").arg(nextExe));
    QProcess::startDetached(nextExe);

    emitMsg(QStringLiteral("Update completed"));
    return 0;
}
