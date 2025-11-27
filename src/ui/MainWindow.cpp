#include "MainWindow.h"

#include "core/CoreService.h"
#include "ui/DynamicForm.h"
#include "core/LoggingBridge.h"

#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(CoreService *core, const QString &toolsRoot, QWidget *parent)
    : QMainWindow(parent)
    , m_core(core)
    , m_toolsRoot(toolsRoot)
    , m_settings(QCoreApplication::organizationName(), QCoreApplication::applicationName())
{
    buildUi();

    connect(m_core, &CoreService::scanFinished, this, &MainWindow::handleScanFinished);
    connect(m_core, &CoreService::jobStarted, this, &MainWindow::handleJobStarted);
    connect(m_core, &CoreService::jobOutput, this, &MainWindow::handleJobOutput);
    connect(m_core, &CoreService::jobFinished, this, &MainWindow::handleJobFinished);
    connect(m_core, &CoreService::envPreparing, this, &MainWindow::handleEnvPreparing);
    connect(m_core, &CoreService::envFailed, this, &MainWindow::handleEnvFailed);
    connect(m_core, &CoreService::envReady, this, &MainWindow::handleEnvReady);

    handleRefreshClicked();
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *layout = new QHBoxLayout(central);

    m_toolList = new QListWidget(central);
    layout->addWidget(m_toolList, 1);

    auto *right = new QWidget(central);
    auto *rightLayout = new QVBoxLayout(right);

    m_form = new DynamicForm(right);
    rightLayout->addWidget(m_form, 2);

    // Advanced options trigger + summary
    auto *advRow = new QWidget(right);
    auto *advLayout = new QHBoxLayout(advRow);
    advLayout->setContentsMargins(0, 0, 0, 0);
    m_advBtn = new QPushButton(QStringLiteral("..."), advRow);
    m_advBtn->setToolTip(tr("高级选项"));
    m_advBtn->setFixedWidth(36);
    m_advSummary = new QLabel(tr("使用工具默认配置"), advRow);
    advLayout->addWidget(m_advBtn, 0, Qt::AlignLeft);
    advLayout->addWidget(m_advSummary, 1);
    advRow->setLayout(advLayout);
    rightLayout->addWidget(advRow);

    auto *outputRow = new QWidget(right);
    auto *outputLayout = new QHBoxLayout(outputRow);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    m_outputDirEdit = new QLineEdit(outputRow);
    m_outputDirEdit->setPlaceholderText(tr("Optional: choose run directory"));
    auto *outputBtn = new QPushButton(tr("Choose Dir"), outputRow);
    connect(outputBtn, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Select Output Directory"));
        if (!dir.isEmpty())
        {
            m_outputDirEdit->setText(dir);
        }
    });
    outputLayout->addWidget(m_outputDirEdit);
    outputLayout->addWidget(outputBtn);
    rightLayout->addWidget(outputRow);
    const QString savedOutput = m_settings.value(QStringLiteral("outputDir")).toString();
    if (!savedOutput.isEmpty())
    {
        m_outputDirEdit->setText(savedOutput);
    }
    connect(m_outputDirEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_settings.setValue(QStringLiteral("outputDir"), text);
    });

    auto *buttonRow = new QWidget(right);
    auto *buttonLayout = new QHBoxLayout(buttonRow);
    buttonLayout->setContentsMargins(0, 0, 0, 0);

    m_runBtn = new QPushButton(tr("Run"), buttonRow);
    m_refreshBtn = new QPushButton(tr("Refresh Tools"), buttonRow);
    buttonLayout->addWidget(m_runBtn);
    buttonLayout->addWidget(m_refreshBtn);
    rightLayout->addWidget(buttonRow);

    m_log = new QTextEdit(right);
    m_log->setReadOnly(true);
    rightLayout->addWidget(m_log, 1);

    layout->addWidget(right, 2);
    setCentralWidget(central);

    connect(m_runBtn, &QPushButton::clicked, this, &MainWindow::handleRunClicked);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::handleRefreshClicked);
    connect(m_toolList, &QListWidget::currentRowChanged, this, &MainWindow::handleToolSelectionChanged);
    connect(m_advBtn, &QPushButton::clicked, this, &MainWindow::handleAdvancedClicked);
    connect(LoggingBridge::instance(), &LoggingBridge::logMessage, this, &MainWindow::handleLogMessage);
}

void MainWindow::handleScanFinished(const ScanResultDTO &result)
{
    m_tools.clear();
    m_toolList->clear();

    if (!result.ok())
    {
        appendLog(QStringLiteral("Scan error: %1").arg(result.error), true);
        return;
    }

    for (const auto &tool : result.tools)
    {
        m_tools.append(tool);
        m_toolList->addItem(QStringLiteral("%1 (%2)").arg(tool.name, tool.id));
    }

    if (!result.tools.isEmpty())
    {
        m_toolList->setCurrentRow(0);
        loadTool(result.tools.first());
    }
}

void MainWindow::handleJobStarted(const QString &toolId, const QString &runDirectory)
{
    appendLog(QStringLiteral("[%1] started, run dir: %2").arg(toolId, runDirectory));
}

void MainWindow::handleJobOutput(const QString &toolId, const QString &line, bool isError)
{
    appendLog(QStringLiteral("[%1] %2").arg(toolId, line), isError);
}

void MainWindow::handleJobFinished(const QString &toolId, int exitCode, const QString &message)
{
    appendLog(QStringLiteral("[%1] finished: %2 (%3)").arg(toolId).arg(exitCode).arg(message), exitCode != 0);
}

void MainWindow::handleEnvPreparing(const QString &toolId)
{
    appendLog(QStringLiteral("[%1] preparing environment...").arg(toolId));
}

void MainWindow::handleEnvFailed(const QString &toolId, const QString &message)
{
    appendLog(QStringLiteral("[%1] env failed: %2").arg(toolId, message), true);
}

void MainWindow::handleEnvReady(const QString &toolId, const QString &envPath)
{
    appendLog(QStringLiteral("[%1] env ready at %2").arg(toolId, envPath));
}

void MainWindow::handleRunClicked()
{
    int idx = m_toolList->currentRow();
    if (idx < 0 || idx >= m_tools.size())
    {
        appendLog(QStringLiteral("No tool selected"), true);
        return;
    }

    const ToolDTO tool = m_tools.at(idx);

    RunRequestDTO req;
    req.toolId = tool.id;
    req.toolVersion = tool.version;
    req.params = m_form->collectValues();
    req.runDirectory = m_outputDirEdit->text();

    const AdvOverride ov = m_advOverrides.value(tool.id, {});
    req.interpreterOverride = ov.program;

    m_core->runTool(m_toolsRoot, tool, req);
}

void MainWindow::handleRefreshClicked()
{
    m_core->startScan(m_toolsRoot);
}

void MainWindow::handleToolSelectionChanged()
{
    const int idx = m_toolList->currentRow();
    if (idx < 0 || idx >= m_tools.size())
    {
        m_form->setParams({});
        m_advSummary->setText(tr("使用工具默认配置"));
        return;
    }
    loadTool(m_tools.at(idx));
}

void MainWindow::appendLog(const QString &text, bool isError)
{
    const QString line = isError ? QStringLiteral("<span style='color:red;'>%1</span>").arg(text) : text;
    m_log->append(line);
}

void MainWindow::loadTool(const ToolDTO &tool)
{
    m_form->setParams(tool.params);
    AdvOverride ov = m_advOverrides.value(tool.id, loadOverride(tool.id));
    m_advOverrides.insert(tool.id, ov);
    updateAdvSummary(tool, ov);
}

void MainWindow::handleAdvancedClicked()
{
    int idx = m_toolList->currentRow();
    if (idx < 0 || idx >= m_tools.size())
    {
        appendLog(QStringLiteral("请选择一个工具后再调整高级选项"), true);
        return;
    }
    const ToolDTO tool = m_tools.at(idx);
    AdvOverride ov = m_advOverrides.value(tool.id, loadOverride(tool.id));

    QDialog dialog(this);
    dialog.setWindowTitle(tr("高级选项"));
    auto *layout = new QVBoxLayout(&dialog);

    auto *progRow = new QWidget(&dialog);
    auto *progLayout = new QHBoxLayout(progRow);
    progLayout->setContentsMargins(0, 0, 0, 0);
    auto *progEdit = new QLineEdit(progRow);
    progEdit->setPlaceholderText(tr("可选：指定解释器/可执行程序路径"));
    progEdit->setText(ov.program);
    auto *progBrowse = new QPushButton(tr("选择"), progRow);
    connect(progBrowse, &QPushButton::clicked, &dialog, [this, progEdit]() {
        const QString path = QFileDialog::getOpenFileName(this, tr("选择可执行文件"));
        if (!path.isEmpty())
        {
            progEdit->setText(path);
        }
    });
    progLayout->addWidget(new QLabel(tr("程序路径"), progRow));
    progLayout->addWidget(progEdit, 1);
    progLayout->addWidget(progBrowse);
    progRow->setLayout(progLayout);
    layout->addWidget(progRow);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted)
    {
        ov.program = progEdit->text().trimmed();
        m_advOverrides.insert(tool.id, ov);
        saveOverride(tool.id, ov);
        updateAdvSummary(tool, ov);
    }
}

void MainWindow::updateAdvSummary(const ToolDTO &tool, const AdvOverride &ov)
{
    Q_UNUSED(tool);
    const QString text = ov.program.isEmpty() ? tr("程序: 默认") : tr("程序: %1").arg(ov.program);
    m_advSummary->setText(text);
}

MainWindow::AdvOverride MainWindow::loadOverride(const QString &toolId)
{
    AdvOverride ov;
    m_settings.beginGroup(QStringLiteral("toolOverrides/%1").arg(toolId));
    ov.program = m_settings.value(QStringLiteral("program")).toString();
    m_settings.endGroup();
    return ov;
}

void MainWindow::saveOverride(const QString &toolId, const AdvOverride &ov)
{
    m_settings.beginGroup(QStringLiteral("toolOverrides/%1").arg(toolId));
    m_settings.setValue(QStringLiteral("program"), ov.program);
    m_settings.endGroup();
}

namespace
{
QString sanitizeLogHtml(const QString &text)
{
    QString safe = text;
    safe.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    safe.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    safe.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    safe.replace(QLatin1Char('\r'), QString());
    safe.replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
    return safe;
}
}

void MainWindow::handleLogMessage(int level, const QString &category, const QString &message)
{
    QString prefix;
    switch (level)
    {
    case QtDebugMsg:
        prefix = "[DEBUG]";
        break;
    case QtInfoMsg:
        prefix = "[INFO]";
        break;
    case QtWarningMsg:
        prefix = "<span style='color:#c97a00;'>[WARN]</span>";
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        prefix = "<span style='color:red;'>[ERROR]</span>";
        break;
    default:
        prefix = "[LOG]";
        break;
    }
    const QString safeCategory = sanitizeLogHtml(category);
    const QString safeMessage = sanitizeLogHtml(message);
    const QString line = QStringLiteral("%1 %2 %3").arg(prefix, safeCategory, safeMessage);
    m_log->append(line);
}
