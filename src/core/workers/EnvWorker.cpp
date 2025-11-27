#include "EnvWorker.h"

#include <QDir>
#include <QLoggingCategory>
#include <QProcess>

Q_LOGGING_CATEGORY(logEnv, "core.env")

namespace
{
bool runCommand(const QString &program, const QStringList &args, const QString &workdir, QString &errorOut, int timeoutMs = 60000)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(args);
    if (!workdir.isEmpty())
    {
        process.setWorkingDirectory(workdir);
    }

    process.start();
    if (!process.waitForFinished(timeoutMs))
    {
        process.kill();
        errorOut = QStringLiteral("Command timed out: %1 %2").arg(program, args.join(' '));
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    {
        const QString stderrText = QString::fromUtf8(process.readAllStandardError());
        const QString stdoutText = QString::fromUtf8(process.readAllStandardOutput());
        errorOut = stderrText;
        if (errorOut.isEmpty())
            errorOut = stdoutText;
        if (errorOut.isEmpty())
            errorOut = QStringLiteral("Command failed: %1 %2").arg(program, args.join(' '));
        return false;
    }
    return true;
}

bool runCommandString(const QString &command, const QString &workdir, bool useShell, QString &errorOut, int timeoutMs = 60000)
{
    if (command.trimmed().isEmpty())
    {
        errorOut = QStringLiteral("Empty command");
        return false;
    }

    if (useShell)
    {
#ifdef Q_OS_WIN
        return runCommand(QStringLiteral("cmd.exe"), {QStringLiteral("/C"), command}, workdir, errorOut, timeoutMs);
#else
        return runCommand(QStringLiteral("sh"), {QStringLiteral("-c"), command}, workdir, errorOut, timeoutMs);
#endif
    }

    QStringList parts = QProcess::splitCommand(command);
    if (parts.isEmpty())
    {
        errorOut = QStringLiteral("Invalid command: %1").arg(command);
        return false;
    }
    QString program = parts.takeFirst();
    return runCommand(program, parts, workdir, errorOut, timeoutMs);
}

bool commandExists(const QString &program)
{
    QString err;
    return runCommand(program, {QStringLiteral("--version")}, QString(), err, 5000);
}
} // namespace

void EnvWorker::prepareEnv(const QString &toolsRoot, const ToolDTO &tool)
{
    const QString toolDir = QDir(toolsRoot).filePath(tool.id);
    QString envPath;
    QString message;

    const bool ok = prepareByStrategy(toolDir, tool, envPath, message);

    if (ok)
    {
        emit envReady(tool.id, envPath);
    }
    else
    {
        qWarning(logEnv) << "env error" << tool.id << message;
        emit envError(tool.id, message);
    }
}

bool EnvWorker::prepareByStrategy(const QString &toolDir, const ToolDTO &tool, QString &envPath, QString &message) const
{
    QString strategy = tool.env.strategy.trimmed().toLower();
    const QString runtimeType = tool.runtime.type.trimmed().toLower();
    if (strategy.isEmpty())
    {
        if (runtimeType == QStringLiteral("python"))
            strategy = QStringLiteral("uv");
        else if (runtimeType == QStringLiteral("r"))
            strategy = QStringLiteral("pak");
        else
            strategy = QStringLiteral("none");
    }

    bool ok = false;
    if (strategy == QStringLiteral("uv"))
    {
        ok = ensureUvEnv(toolDir, tool, envPath, message);
    }
    else if (strategy == QStringLiteral("pak"))
    {
        ok = ensurePakEnv(toolDir, tool, envPath, message);
    }
    else if (strategy == QStringLiteral("custom"))
    {
        envPath = tool.env.cacheDir.isEmpty() ? QString() : QDir(toolDir).filePath(tool.env.cacheDir);
        ok = runSetupCommand(toolDir, tool.env.setup, message);
    }
    else // none or unknown
    {
        envPath = tool.env.cacheDir.isEmpty() ? QString() : QDir(toolDir).filePath(tool.env.cacheDir);
        ok = tool.env.setup.command.isEmpty() ? true : runSetupCommand(toolDir, tool.env.setup, message);
    }

    if (!ok && message.isEmpty())
    {
        message = QStringLiteral("Unknown env strategy: %1").arg(strategy);
    }
    return ok;
}

bool EnvWorker::ensureUvEnv(const QString &toolDir, const ToolDTO &tool, QString &envPath, QString &message) const
{
    envPath = QDir(toolDir).filePath(tool.env.cacheDir.isEmpty() ? QStringLiteral(".venv") : tool.env.cacheDir);

    if (!commandExists(QStringLiteral("uv")))
    {
        message = QStringLiteral("uv is not installed. Please install uv first.");
        return false;
    }

    if (!QDir(envPath).exists())
    {
        if (!runCommand(QStringLiteral("uv"), {QStringLiteral("venv"), envPath}, toolDir, message))
        {
            return false;
        }
    }

    const QStringList deps = tool.env.dependencies;
    if (!deps.isEmpty())
    {
        QStringList args{QStringLiteral("pip"), QStringLiteral("install")};
        args.append(deps);
        if (!runCommand(QStringLiteral("uv"), args, toolDir, message))
        {
            return false;
        }
    }

    if (!tool.env.setup.command.isEmpty())
    {
        return runSetupCommand(toolDir, tool.env.setup, message);
    }
    return true;
}

bool EnvWorker::ensurePakEnv(const QString &toolDir, const ToolDTO &tool, QString &envPath, QString &message) const
{
    envPath = QDir(toolDir).filePath(tool.env.cacheDir.isEmpty() ? QStringLiteral(".r-lib") : tool.env.cacheDir);

    if (!commandExists(QStringLiteral("Rscript")))
    {
        message = QStringLiteral("Rscript is not available. Please install R.");
        return false;
    }

    QDir().mkpath(envPath);

    if (!tool.env.dependencies.isEmpty())
    {
        QStringList quoted;
        for (const auto &dep : tool.env.dependencies)
        {
            quoted << QStringLiteral("\"%1\"").arg(dep);
        }
        const QString depExpr = QStringLiteral("c(%1)").arg(quoted.join(','));
        const QString script = QStringLiteral("if(!requireNamespace('pak', quietly=TRUE)) install.packages('pak'); pak::pkg_install(%1, lib='%2')").arg(depExpr, envPath.replace("\\", "/"));

        QString err;
        if (!runCommand(QStringLiteral("Rscript"), {QStringLiteral("-e"), script}, toolDir, err))
        {
            message = err;
            return false;
        }
    }

    if (!tool.env.setup.command.isEmpty())
    {
        return runSetupCommand(toolDir, tool.env.setup, message);
    }

    return true;
}

bool EnvWorker::runSetupCommand(const QString &toolDir, const SetupCommandDTO &setup, QString &message) const
{
    if (setup.command.isEmpty())
    {
        return true;
    }
    QString err;
    const QString workdir = QDir(toolDir).filePath(setup.workdir.isEmpty() ? QStringLiteral(".") : setup.workdir);
    if (!runCommandString(setup.command, workdir, setup.shell, err))
    {
        message = err;
        return false;
    }
    return true;
}
