#include "MainWindow.h"

#include "core/CoreService.h"
#include "ui/DynamicForm.h"

#include <QCheckBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QToolButton>
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

    // Collapsible advanced section
    m_advToggle = new QToolButton(right);
    m_advToggle->setText(tr("高级选项"));
    m_advToggle->setCheckable(true);
    m_advToggle->setChecked(false);
    m_advToggle->setArrowType(Qt::RightArrow);
    rightLayout->addWidget(m_advToggle);

    m_advContent = new QWidget(right);
    auto *advLayout = new QVBoxLayout(m_advContent);
    advLayout->setContentsMargins(0, 0, 0, 0);

    auto *interpRow = new QWidget(m_advContent);
    auto *interpLayout = new QHBoxLayout(interpRow);
    interpLayout->setContentsMargins(0, 0, 0, 0);
    m_interpreterEdit = new QLineEdit(interpRow);
    m_interpreterEdit->setPlaceholderText(tr("自定义解释器路径（留空自动）"));
    auto *interpBtn = new QPushButton(tr("选择解释器"), interpRow);
    connect(interpBtn, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, tr("选择解释器可执行文件"));
        if (!path.isEmpty())
        {
            m_interpreterEdit->setText(path);
        }
    });
    interpLayout->addWidget(m_interpreterEdit);
    interpLayout->addWidget(interpBtn);
    interpRow->setLayout(interpLayout);

    m_useUvCheck = new QCheckBox(tr("优先使用 uv run"), m_advContent);

    advLayout->addWidget(interpRow);
    advLayout->addWidget(m_useUvCheck);
    m_advContent->setLayout(advLayout);
    m_advContent->setVisible(false);
    rightLayout->addWidget(m_advContent);

    connect(m_advToggle, &QToolButton::toggled, this, [this](bool checked) {
        m_advToggle->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
        m_advContent->setVisible(checked);
    });

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

    if (m_advToggle->isChecked())
    {
        const QString interp = m_interpreterEdit->text().trimmed();
        if (!interp.isEmpty())
        {
            req.interpreterOverride = interp;
        }
        req.hasUseUvOverride = true;
        req.useUvOverride = m_useUvCheck->isChecked();
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
    const bool isPython = tool.env.type.toLower() == QStringLiteral("python");
    m_useUvCheck->setVisible(isPython);
    m_useUvCheck->setChecked(tool.env.useUv);
    m_interpreterEdit->clear();
}
