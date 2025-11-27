#include "JobWorker.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTextStream>

Q_LOGGING_CATEGORY(logJob, "core.job")

namespace
{
QString pythonFromEnv(const QString &envPath)
{
#ifdef Q_OS_WIN
    return QDir(envPath).filePath(QStringLiteral("Scripts/python.exe"));
#else
    return QDir(envPath).filePath(QStringLiteral("bin/python"));
#endif
}

QString quoteArg(const QString &arg)
{
#ifdef Q_OS_WIN
    QString escaped = arg;
    escaped.replace('"', QStringLiteral("\\\""));
    if (escaped.contains(' '))
    {
        escaped = QStringLiteral("\"%1\"").arg(escaped);
    }
    return escaped;
#else
    if (arg.contains(QRegularExpression(QStringLiteral("[\\s\"']"))))
    {
        QString escaped = arg;
        escaped.replace('\'', QStringLiteral("'\"'\"'"));
        return QStringLiteral("'%1'").arg(escaped);
    }
    return arg;
#endif
}

QString joinCommandForShell(const QString &program, const QStringList &args)
{
    QStringList parts;
    parts << quoteArg(program);
    for (const auto &a : args)
    {
        parts << quoteArg(a);
    }
    return parts.join(QLatin1Char(' '));
}

QMap<QString, QStringList> toParamMap(const QList<RunParamValueDTO> &params)
{
    QMap<QString, QStringList> map;
    for (const auto &p : params)
    {
        map.insert(p.key, p.values);
    }
    return map;
}

QString applyTemplate(QString token,
                      const QMap<QString, QStringList> &paramMap,
                      const QString &runDir,
                      const QString &outputDir,
                      const QString &toolDir,
                      const RuntimeConfigDTO &runtime)
{
    auto firstParam = [&](const QString &key) {
        const QStringList vals = paramMap.value(key);
        return vals.isEmpty() ? QString() : vals.first();
    };

    token.replace(QStringLiteral("{{run.outputs}}"), outputDir);
    token.replace(QStringLiteral("{{run.dir}}"), runDir);
    token.replace(QStringLiteral("{{tool.root}}"), toolDir);
    token.replace(QStringLiteral("{{runtime.workdir}}"), runtime.workdir);

    QRegularExpression re(QStringLiteral(R"(\{\{\s*params\.([A-Za-z0-9_\-]+)\s*\}\})"));
    auto it = re.globalMatch(token);
    while (it.hasNext())
    {
        const auto m = it.next();
        const QString key = m.captured(1);
        token.replace(m.captured(0), firstParam(key));
    }
    return token;
}

QStringList renderToken(const QString &token,
                        const QMap<QString, QStringList> &paramMap,
                        const QString &runDir,
                        const QString &outputDir,
                        const QString &toolDir,
                        const RuntimeConfigDTO &runtime)
{
    QRegularExpression single(QStringLiteral(R"(^\{\{\s*params\.([A-Za-z0-9_\-]+)\s*\}\}$)"));
    const auto m = single.match(token);
    if (m.hasMatch())
    {
        const QStringList vals = paramMap.value(m.captured(1));
        if (vals.isEmpty())
        {
            return {QString()};
        }
        return vals;
    }

    return {applyTemplate(token, paramMap, runDir, outputDir, toolDir, runtime)};
}

QStringList expandArgs(const RuntimeConfigDTO &runtime,
                       const QMap<QString, QStringList> &paramMap,
                       const QString &runDir,
                       const QString &outputDir,
                       const QString &toolDir)
{
    QStringList args;
    for (const QString &tpl : runtime.args)
    {
        args.append(renderToken(tpl, paramMap, runDir, outputDir, toolDir, runtime));
    }
    return args;
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
    const QString outputDir = QDir(runDir).filePath(QStringLiteral("outputs"));
    QDir().mkpath(outputDir);

    m_notified = false;
    m_process.reset(new QProcess());
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    wireProcessSignals(*m_process, tool.id, runDir);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove(QStringLiteral("PYTHONHOME"));
    env.remove(QStringLiteral("PYTHONPATH"));
    env.insert(QStringLiteral("PYTHONHOME"), QString());
    env.insert(QStringLiteral("PYTHONPATH"), QString());
    env.insert(QStringLiteral("TOOL_OUTPUT_DIR"), outputDir);
    env.insert(QStringLiteral("TOOL_ROOT"), QDir(toolsRoot).filePath(tool.id));
    env.insert(QStringLiteral("TOOL_RUN_DIR"), runDir);

    for (auto it = tool.runtime.extraEnv.cbegin(); it != tool.runtime.extraEnv.cend(); ++it)
    {
        env.insert(it.key(), it.value());
    }

    QString program;
    QStringList args;

    const QString toolDir = QDir(toolsRoot).filePath(tool.id);
    const QString entryPath = QDir(toolDir).filePath(tool.runtime.entry);
    const QString runtimeType = tool.runtime.type.trimmed().toLower();
    const QMap<QString, QStringList> paramMap = toParamMap(request.params);
    const QStringList templatedArgs = expandArgs(tool.runtime, paramMap, runDir, outputDir, toolDir);

    if (runtimeType == QStringLiteral("python"))
    {
        const QString interpreter = !tool.env.interpreterPath.isEmpty()
                                        ? tool.env.interpreterPath
                                        : (!envPath.isEmpty() ? pythonFromEnv(envPath) : QStringLiteral("python"));
        program = interpreter;
        args << entryPath;
        args << templatedArgs;

        if (!envPath.isEmpty())
        {
#ifdef Q_OS_WIN
            const QString binPath = QDir(envPath).filePath(QStringLiteral("Scripts"));
#else
            const QString binPath = QDir(envPath).filePath(QStringLiteral("bin"));
#endif
            env.insert(QStringLiteral("PATH"), binPath + ";" + env.value(QStringLiteral("PATH")));
            env.insert(QStringLiteral("VIRTUAL_ENV"), envPath);
        }
    }
    else if (runtimeType == QStringLiteral("r"))
    {
        program = tool.env.interpreterPath.isEmpty() ? QStringLiteral("Rscript") : tool.env.interpreterPath;
        args << entryPath;
        args << templatedArgs;
        if (!envPath.isEmpty())
        {
            env.insert(QStringLiteral("R_LIBS_USER"), envPath);
        }
    }
    else // generic
    {
        program = entryPath;
        args = templatedArgs;
    }

    if (tool.runtime.shellWrap)
    {
        const QString commandLine = joinCommandForShell(program, args);
#ifdef Q_OS_WIN
        program = QStringLiteral("cmd.exe");
        args = {QStringLiteral("/C"), commandLine};
#else
        program = QStringLiteral("sh");
        args = {QStringLiteral("-c"), commandLine};
#endif
    }

    m_process->setProcessEnvironment(env);
    m_process->setWorkingDirectory(runDir);

    emit jobStarted(tool.id, runDir);
    m_process->start(program, args);

    if (!m_process->waitForStarted(5000))
    {
        const QString err = m_process->errorString();
        emit jobFinished(tool.id, -1, QStringLiteral("Failed to start: %1").arg(err));
        return;
    }
    qInfo(logJob) << "Started" << tool.id << "program" << program << "args" << args << "runDir" << runDir;
}

void JobWorker::cancel()
{
    if (m_process && m_process->state() != QProcess::NotRunning)
    {
        m_process->terminate();
    }
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
    QDir(runDir).mkpath(QStringLiteral("outputs"));
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
                         if (m_notified)
                             return;
                         m_notified = true;
                         const QString message = status == QProcess::NormalExit
                                                     ? QStringLiteral("exit %1").arg(exitCode)
                                                     : QStringLiteral("crashed");
                         emit jobFinished(toolId, exitCode, message);
                         qInfo(logJob) << "Finished" << toolId << "exit" << exitCode << "status" << (status == QProcess::NormalExit);
                     });

    QObject::connect(&process, &QProcess::errorOccurred, &process, [this, toolId](QProcess::ProcessError error) {
        if (m_notified)
            return;
        m_notified = true;
        const QString msg = QStringLiteral("Process error: %1").arg(static_cast<int>(error));
        emit jobFinished(toolId, -1, msg);
        qWarning(logJob) << "Error" << toolId << msg;
    });
}
