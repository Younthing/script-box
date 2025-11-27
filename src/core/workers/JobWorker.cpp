#include "JobWorker.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QProcessEnvironment>
#include <QTextStream>

namespace
{
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
} // namespace

void JobWorker::runJob(const QString &toolsRoot, const ToolDTO &tool, const RunRequestDTO &request)
{
    if (m_process && m_process->state() != QProcess::NotRunning)
    {
        emit jobFinished(tool.id, -1, QStringLiteral("A job is already running"));
        return;
    }

    const QString runDir = ensureRunDirectory(toolsRoot, tool, request);

    m_process.reset(new QProcess());
    appendArguments(*m_process, tool, request);
    wireProcessSignals(*m_process, tool.id, runDir);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (auto it = tool.env.envVars.cbegin(); it != tool.env.envVars.cend(); ++it)
    {
        env.insert(it.key(), it.value());
    }
    m_process->setProcessEnvironment(env);

    const QString program = resolveProgram(QDir(toolsRoot).filePath(tool.id), tool.command);
    if (program.isEmpty())
    {
        emit jobFinished(tool.id, -1, QStringLiteral("Invalid command"));
        return;
    }

    QStringList commandParts = QProcess::splitCommand(tool.command);
    if (!commandParts.isEmpty())
    {
        commandParts.takeFirst(); // remove program element
    }
    commandParts.append(buildCliArgs(request));

    const QString workingDir = request.workdir.isEmpty() ? runDir : QDir(runDir).filePath(request.workdir);
    m_process->setWorkingDirectory(workingDir);

    emit jobStarted(tool.id, runDir);
    m_process->start(program, commandParts);
}

void JobWorker::cancel()
{
    if (m_process && m_process->state() != QProcess::NotRunning)
    {
        m_process->terminate();
    }
}

void JobWorker::appendArguments(QProcess &process, const ToolDTO &tool, const RunRequestDTO &request) const
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

    QObject::connect(&process, &QProcess::readyReadStandardOutput, &process, [this, &process, stdoutFile, toolId]() {
        const QByteArray data = process.readAllStandardOutput();
        stdoutFile->write(data);
        stdoutFile->flush();
        const QString text = QString::fromUtf8(data);
        const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
        for (const QString &line : lines)
        {
            emit jobOutput(toolId, line.trimmed(), false);
        }
    });

    QObject::connect(&process, &QProcess::readyReadStandardError, &process, [this, &process, stderrFile, toolId]() {
        const QByteArray data = process.readAllStandardError();
        stderrFile->write(data);
        stderrFile->flush();
        const QString text = QString::fromUtf8(data);
        const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
        for (const QString &line : lines)
        {
            emit jobOutput(toolId, line.trimmed(), true);
        }
    });

    QObject::connect(&process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &process,
                     [this, stdoutFile, stderrFile, toolId](int exitCode, QProcess::ExitStatus status) {
                         stdoutFile->close();
                         stderrFile->close();
                         const QString message = status == QProcess::NormalExit
                                                     ? QStringLiteral("exit %1").arg(exitCode)
                                                     : QStringLiteral("crashed");
                         emit jobFinished(toolId, exitCode, message);
                     });
}
