#include "ToolWindow.h"

#include "core/CoreService.h"
#include "ui/DynamicForm.h"

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

ToolWindow::ToolWindow(CoreService *core, const QString &toolsRoot, const ToolDTO &tool, QWidget *parent)
    : QMainWindow(parent)
    , m_core(core)
    , m_toolsRoot(toolsRoot)
    , m_tool(tool)
    , m_settings(QCoreApplication::organizationName(), QCoreApplication::applicationName())
{
    m_override = loadOverride();
    buildUi();

    connect(m_core, &CoreService::jobStarted, this, &ToolWindow::handleJobStarted);
    connect(m_core, &CoreService::jobOutput, this, &ToolWindow::handleJobOutput);
    connect(m_core, &CoreService::jobFinished, this, &ToolWindow::handleJobFinished);
    connect(m_core, &CoreService::envPreparing, this, &ToolWindow::handleEnvPreparing);
    connect(m_core, &CoreService::envFailed, this, &ToolWindow::handleEnvFailed);
    connect(m_core, &CoreService::envReady, this, &ToolWindow::handleEnvReady);
}

void ToolWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *title = new QLabel(QStringLiteral("%1 (%2)").arg(m_tool.name, m_tool.id), central);
    title->setStyleSheet(QStringLiteral("font-size:18px;font-weight:bold;"));
    layout->addWidget(title);

    if (!m_tool.description.isEmpty())
    {
        auto *desc = new QLabel(m_tool.description, central);
        desc->setWordWrap(true);
        desc->setStyleSheet(QStringLiteral("color:#555;"));
        layout->addWidget(desc);
    }

    m_form = new DynamicForm(central);
    m_form->setParams(m_tool.params);
    layout->addWidget(m_form, 0);

    auto *advRow = new QWidget(central);
    auto *advLayout = new QHBoxLayout(advRow);
    advLayout->setContentsMargins(0, 0, 0, 0);
    m_advBtn = new QPushButton(tr("高级"), advRow);
    m_advSummary = new QLabel(tr("程序: 默认"), advRow);
    advLayout->addWidget(m_advBtn, 0);
    advLayout->addWidget(m_advSummary, 1);
    layout->addWidget(advRow);

    auto *outRow = new QWidget(central);
    auto *outLayout = new QHBoxLayout(outRow);
    outLayout->setContentsMargins(0, 0, 0, 0);
    m_outputDirEdit = new QLineEdit(outRow);
    m_outputDirEdit->setPlaceholderText(tr("可选：运行输出目录"));
    auto *outBtn = new QPushButton(tr("选择"), outRow);
    connect(outBtn, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("选择运行目录"));
        if (!dir.isEmpty())
        {
            m_outputDirEdit->setText(dir);
        }
    });
    outLayout->addWidget(new QLabel(tr("运行目录"), outRow));
    outLayout->addWidget(m_outputDirEdit, 1);
    outLayout->addWidget(outBtn);
    layout->addWidget(outRow);

    auto *btnRow = new QWidget(central);
    auto *btnLayout = new QHBoxLayout(btnRow);
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->addStretch(1);
    m_runBtn = new QPushButton(tr("运行"), btnRow);
    btnLayout->addWidget(m_runBtn);
    btnRow->setLayout(btnLayout);
    layout->addWidget(btnRow);

    m_log = new QTextEdit(central);
    m_log->setReadOnly(true);
    m_log->setMinimumHeight(200);
    layout->addWidget(m_log, 1);

    setCentralWidget(central);
    setMinimumSize(640, 480);
    updateAdvSummary(m_override);

    connect(m_runBtn, &QPushButton::clicked, this, &ToolWindow::handleRunClicked);
    connect(m_advBtn, &QPushButton::clicked, this, &ToolWindow::handleAdvancedClicked);
}

void ToolWindow::handleRunClicked()
{
    RunRequestDTO req;
    req.toolId = m_tool.id;
    req.toolVersion = m_tool.version;
    req.params = m_form->collectValues();
    req.runDirectory = m_outputDirEdit->text();
    req.interpreterOverride = m_override.program;

    appendLog(tr("开始运行..."));
    m_core->runTool(m_toolsRoot, m_tool, req);
}

void ToolWindow::handleAdvancedClicked()
{
    AdvOverride ov = m_override;

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
        m_override = ov;
        saveOverride(ov);
        updateAdvSummary(ov);
    }
}

void ToolWindow::handleJobStarted(const QString &toolId, const QString &runDirectory)
{
    if (toolId != m_tool.id)
        return;
    appendLog(tr("已启动，运行目录：%1").arg(runDirectory));
}

void ToolWindow::handleJobOutput(const QString &toolId, const QString &line, bool isError)
{
    if (toolId != m_tool.id)
        return;
    appendLog(line, isError);
}

void ToolWindow::handleJobFinished(const QString &toolId, int exitCode, const QString &message)
{
    if (toolId != m_tool.id)
        return;
    appendLog(tr("完成：%1 (%2)").arg(exitCode).arg(message), exitCode != 0);
}

void ToolWindow::handleEnvPreparing(const QString &toolId)
{
    if (toolId != m_tool.id)
        return;
    appendLog(tr("环境准备中..."));
}

void ToolWindow::handleEnvFailed(const QString &toolId, const QString &message)
{
    if (toolId != m_tool.id)
        return;
    appendLog(tr("环境失败：%1").arg(message), true);
}

void ToolWindow::handleEnvReady(const QString &toolId, const QString &envPath)
{
    if (toolId != m_tool.id)
        return;
    appendLog(tr("环境就绪：%1").arg(envPath));
}

void ToolWindow::appendLog(const QString &text, bool isError)
{
    const QString line = isError ? QStringLiteral("<span style='color:red;'>%1</span>").arg(text) : text;
    m_log->append(line);
}

void ToolWindow::updateAdvSummary(const AdvOverride &ov)
{
    const QString text = ov.program.isEmpty() ? tr("程序: 默认") : tr("程序: %1").arg(ov.program);
    m_advSummary->setText(text);
}

ToolWindow::AdvOverride ToolWindow::loadOverride()
{
    AdvOverride ov;
    m_settings.beginGroup(QStringLiteral("toolOverrides/%1").arg(m_tool.id));
    ov.program = m_settings.value(QStringLiteral("program")).toString();
    m_settings.endGroup();
    return ov;
}

void ToolWindow::saveOverride(const AdvOverride &ov)
{
    m_settings.beginGroup(QStringLiteral("toolOverrides/%1").arg(m_tool.id));
    m_settings.setValue(QStringLiteral("program"), ov.program);
    m_settings.endGroup();
}
