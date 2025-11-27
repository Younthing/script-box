#include "EnvWorker.h"

#include <QDir>
#include <QProcess>

namespace
{
bool runCommand(const QString &program, const QStringList &args, const QString &workdir, QString &errorOut)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(args);
    if (!workdir.isEmpty())
    {
        process.setWorkingDirectory(workdir);
    }

    process.start();
    process.waitForFinished(-1);

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    {
        errorOut = QString::fromUtf8(process.readAllStandardError());
        if (errorOut.isEmpty())
        {
            errorOut = QStringLiteral("Command failed: %1 %2").arg(program, args.join(' '));
        }
        return false;
    }
    return true;
}

bool commandExists(const QString &program)
{
    QString err;
    return runCommand(program, {QStringLiteral("--version")}, QString(), err);
}
} // namespace

void EnvWorker::prepareEnv(const QString &toolsRoot, const ToolDTO &tool)
{
    const QString toolDir = QDir(toolsRoot).filePath(tool.id);
    QString envPath;
    QString message;

    const QString envType = tool.env.type.trimmed().toLower();
    bool ok = true;

    if (envType == QStringLiteral("python"))
    {
        ok = ensurePythonEnv(toolDir, tool, envPath, message);
    }
    else if (envType == QStringLiteral("r"))
    {
        ok = ensureREnv(toolDir, tool, envPath, message);
    }
    else
    {
        envPath.clear();
    }

    if (ok)
    {
        emit envReady(tool.id, envPath);
    }
    else
    {
        emit envError(tool.id, message);
    }
}

bool EnvWorker::ensurePythonEnv(const QString &toolDir, const ToolDTO &tool, QString &envPath, QString &message) const
{
    envPath = QDir(toolDir).filePath(QStringLiteral(".venv"));

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

    QStringList deps = tool.env.dependencies;
    if (!deps.isEmpty())
    {
        QStringList args{QStringLiteral("pip"), QStringLiteral("install")};
        args.append(deps);
        if (!runCommand(QStringLiteral("uv"), args, toolDir, message))
        {
            return false;
        }
    }

    return true;
}

bool EnvWorker::ensureREnv(const QString &toolDir, const ToolDTO &tool, QString &envPath, QString &message) const
{
    envPath = QDir(toolDir).filePath(QStringLiteral(".r-lib"));

    if (!commandExists(QStringLiteral("Rscript")))
    {
        message = QStringLiteral("Rscript is not available. Please install R.");
        return false;
    }

    QDir().mkpath(envPath);

    if (tool.env.dependencies.isEmpty())
    {
        return true;
    }

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

    return true;
}
