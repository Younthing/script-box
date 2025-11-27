#include "MainWindow.h"

#include "core/CoreService.h"
#include "ui/DynamicForm.h"

#include <QCheckBox>
#include <QComboBox>
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
        m_tools.insert(tool.id, tool);
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

    const QString key = m_tools.keys().at(idx);
    const ToolDTO tool = m_tools.value(key);

    RunRequestDTO req;
    req.toolId = tool.id;
    req.toolVersion = tool.version;
    req.params = m_form->collectValues();
    req.workdir = tool.workdir;
    req.runDirectory = m_outputDirEdit->text();

    if (!m_advInterpreter.isEmpty())
    {
        req.interpreterOverride = m_advInterpreter;
    }
    if (m_hasUvOverride)
    {
        req.hasUseUvOverride = true;
        req.useUvOverride = m_uvOverride;
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
        m_advInterpreter.clear();
        m_hasUvOverride = false;
        m_uvOverride = false;
        m_advSummary->setText(tr("使用工具默认配置"));
        return;
    }
    const QString key = m_tools.keys().at(idx);
    loadTool(m_tools.value(key));
}

void MainWindow::appendLog(const QString &text, bool isError)
{
    const QString line = isError ? QStringLiteral("<span style='color:red;'>%1</span>").arg(text) : text;
    m_log->append(line);
}

void MainWindow::loadTool(const ToolDTO &tool)
{
    m_form->setParams(tool.params);
    m_advInterpreter.clear();
    m_hasUvOverride = false;
    m_uvOverride = false;
    m_advSummary->setText(tr("使用工具默认配置"));
}

void MainWindow::handleAdvancedClicked()
{
    int idx = m_toolList->currentRow();
    if (idx < 0 || idx >= m_tools.size())
    {
        appendLog(QStringLiteral("请选择一个工具后再调整高级选项"), true);
        return;
    }
    const QString key = m_tools.keys().at(idx);
    const ToolDTO tool = m_tools.value(key);
    const bool isPython = tool.env.type.toLower() == QStringLiteral("python");

    QDialog dialog(this);
    dialog.setWindowTitle(tr("高级选项"));
    auto *layout = new QVBoxLayout(&dialog);

    auto *interpRow = new QWidget(&dialog);
    auto *interpLayout = new QHBoxLayout(interpRow);
    interpLayout->setContentsMargins(0, 0, 0, 0);
    auto *interpEdit = new QLineEdit(interpRow);
    interpEdit->setPlaceholderText(tr("自定义解释器路径（留空自动）"));
    interpEdit->setText(m_advInterpreter);
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
    if (m_hasUvOverride)
    {
        current = m_uvOverride ? QStringLiteral("on") : QStringLiteral("off");
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
        m_advInterpreter = interpEdit->text().trimmed();

        const QString uvChoice = uvCombo->currentData().toString();
        if (uvChoice == QStringLiteral("auto") || !isPython)
        {
            m_hasUvOverride = false;
            m_uvOverride = false;
        }
        else
        {
            m_hasUvOverride = true;
            m_uvOverride = (uvChoice == QStringLiteral("on"));
        }

        QString summary = m_advInterpreter.isEmpty() ? tr("解释器: 默认") : tr("解释器: %1").arg(m_advInterpreter);
        if (isPython)
        {
            summary += tr(" | uv: ");
            if (!m_hasUvOverride)
                summary += tr("默认");
            else
                summary += m_uvOverride ? tr("强制启用") : tr("禁用");
        }
        m_advSummary->setText(summary);
    }
}
