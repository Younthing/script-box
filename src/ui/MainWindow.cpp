#include "MainWindow.h"

#include "core/CoreService.h"
#include "ui/DynamicForm.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QDialog>
#include <QDialogButtonBox>
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
    m_outputDirEdit->setPlaceholderText(tr("Optional: choose output/run directory"));
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
    req.workdir = tool.workdir;
    req.runDirectory = m_outputDirEdit->text();

    const AdvOverride ov = m_advOverrides.value(tool.id, {});
    if (!ov.interpreter.isEmpty())
    {
        req.interpreterOverride = ov.interpreter;
    }
    if (ov.hasUv)
    {
        req.hasUseUvOverride = true;
        req.useUvOverride = ov.uv;
    }

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
    AdvOverride ov = m_advOverrides.value(tool.id, {});
    const bool isPython = tool.env.type.toLower() == QStringLiteral("python");

    QDialog dialog(this);
    dialog.setWindowTitle(tr("高级选项"));
    auto *layout = new QVBoxLayout(&dialog);

    auto *interpRow = new QWidget(&dialog);
    auto *interpLayout = new QHBoxLayout(interpRow);
    interpLayout->setContentsMargins(0, 0, 0, 0);
    auto *interpEdit = new QLineEdit(interpRow);
    interpEdit->setPlaceholderText(tr("自定义解释器路径（留空自动）"));
    interpEdit->setText(ov.interpreter);
    auto *browseBtn = new QPushButton(tr("选择"), interpRow);
    connect(browseBtn, &QPushButton::clicked, &dialog, [this, interpEdit]() {
        const QString path = QFileDialog::getOpenFileName(this, tr("选择解释器可执行文件"));
        if (!path.isEmpty())
        {
            interpEdit->setText(path);
        }
    });
    interpLayout->addWidget(new QLabel(tr("解释器"), interpRow));
    interpLayout->addWidget(interpEdit);
    interpLayout->addWidget(browseBtn);
    interpRow->setLayout(interpLayout);
    layout->addWidget(interpRow);

    auto *uvRow = new QWidget(&dialog);
    auto *uvLayout = new QHBoxLayout(uvRow);
    uvLayout->setContentsMargins(0, 0, 0, 0);
    auto *uvCombo = new QComboBox(uvRow);
    uvCombo->addItem(tr("遵循工具默认"), QStringLiteral("auto"));
    uvCombo->addItem(tr("强制使用 uv run"), QStringLiteral("on"));
    uvCombo->addItem(tr("禁用 uv run"), QStringLiteral("off"));
    QString current = QStringLiteral("auto");
    if (ov.hasUv)
    {
        current = ov.uv ? QStringLiteral("on") : QStringLiteral("off");
    }
    const int idxCombo = uvCombo->findData(current);
    if (idxCombo >= 0)
    {
        uvCombo->setCurrentIndex(idxCombo);
    }
    uvCombo->setEnabled(isPython);
    uvLayout->addWidget(new QLabel(tr("uv 选项"), uvRow));
    uvLayout->addWidget(uvCombo, 1);
    uvRow->setLayout(uvLayout);
    layout->addWidget(uvRow);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted)
    {
        ov.interpreter = interpEdit->text().trimmed();

        const QString uvChoice = uvCombo->currentData().toString();
        if (uvChoice == QStringLiteral("auto") || !isPython)
        {
            ov.hasUv = false;
            ov.uv = false;
        }
        else
        {
            ov.hasUv = true;
            ov.uv = (uvChoice == QStringLiteral("on"));
        }

        QString summary = ov.interpreter.isEmpty() ? tr("解释器: 默认") : tr("解释器: %1").arg(ov.interpreter);
        if (isPython)
        {
            summary += tr(" | uv: ");
            if (!ov.hasUv)
                summary += tool.env.useUv ? tr("默认(启用)") : tr("默认(禁用)");
            else
                summary += ov.uv ? tr("强制启用") : tr("禁用");
        }
        m_advSummary->setText(summary);
        m_advOverrides.insert(tool.id, ov);
        saveOverride(tool.id, ov);
    }
}

void MainWindow::updateAdvSummary(const ToolDTO &tool, const AdvOverride &ov)
{
    QString summary = tr("使用工具默认配置");
    if (!ov.interpreter.isEmpty())
    {
        summary = tr("解释器: %1").arg(ov.interpreter);
    }
    if (tool.env.type.toLower() == QStringLiteral("python"))
    {
        summary += tr(" | uv: ");
        if (!ov.hasUv)
            summary += tool.env.useUv ? tr("默认(启用)") : tr("默认(禁用)");
        else
            summary += ov.uv ? tr("强制启用") : tr("禁用");
    }
    m_advSummary->setText(summary);
}

MainWindow::AdvOverride MainWindow::loadOverride(const QString &toolId)
{
    AdvOverride ov;
    m_settings.beginGroup(QStringLiteral("toolOverrides/%1").arg(toolId));
    ov.interpreter = m_settings.value(QStringLiteral("interpreter")).toString();
    ov.hasUv = m_settings.value(QStringLiteral("hasUv"), false).toBool();
    ov.uv = m_settings.value(QStringLiteral("uv"), false).toBool();
    m_settings.endGroup();
    return ov;
}

void MainWindow::saveOverride(const QString &toolId, const AdvOverride &ov)
{
    m_settings.beginGroup(QStringLiteral("toolOverrides/%1").arg(toolId));
    m_settings.setValue(QStringLiteral("interpreter"), ov.interpreter);
    m_settings.setValue(QStringLiteral("hasUv"), ov.hasUv);
    m_settings.setValue(QStringLiteral("uv"), ov.uv);
    m_settings.endGroup();
}
