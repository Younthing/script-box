#include "JobWorker.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QProcessEnvironment>
#include <QTextStream>

namespace
{
static QString pythonFromEnv(const QString &envPath)
{
#ifdef Q_OS_WIN
    return QDir(envPath).filePath(QStringLiteral("Scripts/python.exe"));
#else
    return QDir(envPath).filePath(QStringLiteral("bin/python"));
#endif
}

QStringList buildCliArgs(const RunRequestDTO &request)
{
    QStringList args;
    for (const auto &p : request.params)
    {
        for (const QString &val : p.values)
        {
            args << QStringLiteral("--%1=%2").arg(p.key, val);
        }
    }
    return args;
}

QString resolveProgram(const QString &toolDir, const QString &command)
{
    QStringList parts = QProcess::splitCommand(command);
    if (parts.isEmpty())
    {
        return {};
    }

    QString program = parts.takeFirst();
    if (QDir::isRelativePath(program))
    {
        program = QDir(toolDir).filePath(program);
    }
    return program;
}

QStringList resolveArgs(const QString &toolDir, const QString &command)
{
    QStringList parts = QProcess::splitCommand(command);
    if (parts.isEmpty())
    {
        return {};
    }
    parts.takeFirst(); // program element
    if (!parts.isEmpty())
    {
        QString &first = parts.first();
        if (QDir::isRelativePath(first))
        {
            first = QDir(toolDir).filePath(first);
        }
    }
    return parts;
}
} // namespace

void JobWorker::runJob(const QString &toolsRoot, const ToolDTO &tool, const RunRequestDTO &request, const QString &envPath)
{
    if (m_process && m_process->state() != QProcess::NotRunning)
    {
        emit jobFinished(tool.id, -1, QStringLiteral("A job is already running"));
        return;
    }

    const QString runDir = ensureRunDirectory(toolsRoot, tool, request);

    m_process.reset(new QProcess());
    appendArguments(*m_process, tool, request, envPath);
    wireProcessSignals(*m_process, tool.id, runDir);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("PYTHONHOME"), QString());
    env.insert(QStringLiteral("PYTHONPATH"), QString());

    if (!envPath.isEmpty())
    {
        QString binPath = envPath;
        if (tool.env.type.toLower() == QStringLiteral("python"))
        {
            binPath = QDir(envPath).filePath(QStringLiteral("Scripts"));
        }
        env.insert(QStringLiteral("PATH"), binPath + ";" + env.value(QStringLiteral("PATH")));
    }
    for (auto it = tool.env.envVars.cbegin(); it != tool.env.envVars.cend(); ++it)
    {
        env.insert(it.key(), it.value());
    }
    env.insert(QStringLiteral("TOOL_OUTPUT_DIR"), runDir);
    m_process->setProcessEnvironment(env);

    const QString toolDir = QDir(toolsRoot).filePath(tool.id);
    QString program = resolveProgram(toolDir, tool.command);
    QStringList commandParts = resolveArgs(toolDir, tool.command);

    const QString envPython = !envPath.isEmpty() ? pythonFromEnv(envPath) : QString();
    const QString envType = tool.env.type.toLower();

    // overrides
    QString interpreter = !request.interpreterOverride.isEmpty() ? request.interpreterOverride : tool.env.interpreter;
    bool useUv = tool.env.useUv;
    if (request.hasUseUvOverride)
    {
        useUv = request.useUvOverride;
    }

    if (!interpreter.isEmpty())
    {
        program = interpreter;
    }
    else if (envType == QStringLiteral("python") && !envPython.isEmpty())
    {
        program = envPython;
    }

    if (envType == QStringLiteral("python") && useUv)
    {
        QStringList uvArgs;
        uvArgs << QStringLiteral("run");
        if (!interpreter.isEmpty())
        {
            uvArgs << QStringLiteral("--python") << interpreter;
        }
        else if (!envPython.isEmpty())
        {
            uvArgs << QStringLiteral("--python") << envPython;
        }
        uvArgs << program;
        uvArgs << commandParts;

        program = QStringLiteral("uv");
        commandParts = uvArgs;
    }

    if (program.isEmpty())
    {
        emit jobFinished(tool.id, -1, QStringLiteral("Invalid command"));
        return;
    }

    commandParts.append(buildCliArgs(request));

    const QString workingDir = request.workdir.isEmpty() ? runDir : QDir(runDir).filePath(request.workdir);
    m_process->setWorkingDirectory(workingDir);

    emit jobStarted(tool.id, runDir);
    m_process->start(program, commandParts);

    if (!m_process->waitForStarted(5000))
    {
        const QString err = m_process->errorString();
        emit jobFinished(tool.id, -1, QStringLiteral("Failed to start: %1").arg(err));
        return;
    }
}

void JobWorker::cancel()
{
    if (m_process && m_process->state() != QProcess::NotRunning)
    {
        m_process->terminate();
    }
}

void JobWorker::appendArguments(QProcess &process, const ToolDTO &tool, const RunRequestDTO &request, const QString &envPath) const
{
    Q_UNUSED(tool);
    Q_UNUSED(request);
    process.setProcessChannelMode(QProcess::SeparateChannels);
}

QString JobWorker::ensureRunDirectory(const QString &toolsRoot, const ToolDTO &tool, const RunRequestDTO &request) const
{
    QString runDir = request.runDirectory;
    if (runDir.isEmpty())
    {
        const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_hh-mm-ss"));
        runDir = QDir(toolsRoot).filePath(QStringLiteral("runs/%1_%2").arg(timestamp, tool.id));
    }

    QDir().mkpath(runDir);
    QDir(runDir).mkpath(QStringLiteral("logs"));
    return QDir(runDir).absolutePath();
}

void JobWorker::wireProcessSignals(QProcess &process, const QString &toolId, const QString &runDir)
{
    auto stdoutPath = QDir(runDir).filePath(QStringLiteral("logs/stdout.log"));
    auto stderrPath = QDir(runDir).filePath(QStringLiteral("logs/stderr.log"));

    auto *stdoutFile = new QFile(stdoutPath, &process);
    auto *stderrFile = new QFile(stderrPath, &process);
    stdoutFile->open(QIODevice::WriteOnly | QIODevice::Text);
    stderrFile->open(QIODevice::WriteOnly | QIODevice::Text);

    QObject::connect(&process, &QProcess::readyReadStandardOutput, &process, [this, &process, stdoutFile, toolId]()
                     {
        const QByteArray data = process.readAllStandardOutput();
        stdoutFile->write(data);
        stdoutFile->flush();
        const QString text = QString::fromUtf8(data);
        const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
        for (const QString &line : lines)
        {
            emit jobOutput(toolId, line.trimmed(), false);
        } });

    QObject::connect(&process, &QProcess::readyReadStandardError, &process, [this, &process, stderrFile, toolId]()
                     {
        const QByteArray data = process.readAllStandardError();
        stderrFile->write(data);
        stderrFile->flush();
        const QString text = QString::fromUtf8(data);
        const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
        for (const QString &line : lines)
        {
            emit jobOutput(toolId, line.trimmed(), true);
        } });

    QObject::connect(&process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &process,
                     [this, stdoutFile, stderrFile, toolId](int exitCode, QProcess::ExitStatus status)
                     {
                         stdoutFile->close();
                         stderrFile->close();
                         const QString message = status == QProcess::NormalExit
                                                     ? QStringLiteral("exit %1").arg(exitCode)
                                                     : QStringLiteral("crashed");
                         emit jobFinished(toolId, exitCode, message);
                     });

    QObject::connect(&process, &QProcess::errorOccurred, &process, [this, toolId](QProcess::ProcessError error) {
        emit jobFinished(toolId, -1, QStringLiteral("Process error: %1").arg(static_cast<int>(error)));
    });
}
