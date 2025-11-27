#include "MainWindow.h"

#include "core/CoreService.h"
#include "ui/ToolWindow.h"

#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
const QString kAllCategory = QStringLiteral("全部");
} // namespace

MainWindow::MainWindow(CoreService *core, const QString &toolsRoot, QWidget *parent)
    : QMainWindow(parent)
    , m_core(core)
    , m_toolsRoot(toolsRoot)
{
    buildUi();

    connect(m_core, &CoreService::scanFinished, this, &MainWindow::handleScanFinished);

    handleRefreshClicked();
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *layout = new QHBoxLayout(central);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    m_categoryList = new QListWidget(central);
    m_categoryList->setMaximumWidth(180);
    layout->addWidget(m_categoryList);

    auto *right = new QWidget(central);
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(6);

    auto *toolbar = new QWidget(right);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(6);

    m_summaryLabel = new QLabel(tr("工具：0"), toolbar);
    m_refreshBtn = new QPushButton(tr("刷新"), toolbar);
    m_toggleViewBtn = new QPushButton(tr("切换列表/卡片"), toolbar);

    toolbarLayout->addWidget(m_summaryLabel, 1);
    toolbarLayout->addWidget(m_toggleViewBtn, 0);
    toolbarLayout->addWidget(m_refreshBtn, 0);
    toolbar->setLayout(toolbarLayout);
    rightLayout->addWidget(toolbar, 0);

    m_toolList = new QListWidget(right);
    m_toolList->setResizeMode(QListView::Adjust);
    m_toolList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_toolList->setSpacing(8);
    rightLayout->addWidget(m_toolList, 1);

    layout->addWidget(right, 1);
    setCentralWidget(central);

    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::handleRefreshClicked);
    connect(m_categoryList, &QListWidget::currentRowChanged, this, &MainWindow::handleCategoryChanged);
    connect(m_toolList, &QListWidget::itemDoubleClicked, this, &MainWindow::handleToolActivated);
    connect(m_toggleViewBtn, &QPushButton::clicked, this, &MainWindow::handleToggleView);
}

void MainWindow::handleScanFinished(const ScanResultDTO &result)
{
    if (!result.ok())
    {
        QMessageBox::warning(this, tr("扫描失败"), result.error);
        return;
    }
    m_tools = result.tools;
    rebuildCategories();
    rebuildToolList();
}

void MainWindow::handleRefreshClicked()
{
    m_core->startScan(m_toolsRoot);
}

void MainWindow::handleCategoryChanged()
{
    rebuildToolList();
}

void MainWindow::handleToolActivated(QListWidgetItem *item)
{
    if (!item)
        return;
    const QString toolId = item->data(Qt::UserRole).toString();
    for (const auto &tool : m_tools)
    {
        if (tool.id == toolId)
        {
            openToolWindow(tool);
            break;
        }
    }
}

void MainWindow::handleToggleView()
{
    m_cardMode = !m_cardMode;
    rebuildToolList();
}

void MainWindow::rebuildCategories()
{
    m_categoryList->clear();
    QStringList categories;
    categories << kAllCategory;
    for (const auto &tool : m_tools)
    {
        if (!categories.contains(tool.category))
        {
            categories << tool.category;
        }
    }
    categories.sort();
    for (const auto &c : categories)
    {
        m_categoryList->addItem(c);
    }
    m_categoryList->setCurrentRow(0);
}

QList<ToolDTO> MainWindow::filteredTools() const
{
    const QString selected = m_categoryList->currentItem() ? m_categoryList->currentItem()->text() : kAllCategory;
    if (selected == kAllCategory)
    {
        return m_tools;
    }
    QList<ToolDTO> filtered;
    for (const auto &tool : m_tools)
    {
        if (tool.category == selected)
        {
            filtered.append(tool);
        }
    }
    return filtered;
}

QIcon MainWindow::loadIconFor(const ToolDTO &tool) const
{
    QString iconPath;
    if (!tool.thumbnail.isEmpty())
    {
        iconPath = QDir(QDir(m_toolsRoot).filePath(tool.id)).filePath(tool.thumbnail);
    }
    if (iconPath.isEmpty() || !QFileInfo::exists(iconPath))
    {
        iconPath = QDir(m_toolsRoot).absoluteFilePath(QStringLiteral("../assets/tool_placeholder.png"));
    }
    return QIcon(iconPath);
}

void MainWindow::rebuildToolList()
{
    m_toolList->clear();
    const QList<ToolDTO> display = filteredTools();

    m_toolList->setViewMode(m_cardMode ? QListView::IconMode : QListView::ListMode);
    if (m_cardMode)
    {
        m_toolList->setGridSize(QSize(220, 180));
        m_toolList->setIconSize(QSize(200, 120));
    }
    else
    {
        m_toolList->setGridSize(QSize());
        m_toolList->setIconSize(QSize(64, 64));
    }

    for (const auto &tool : display)
    {
        auto *item = new QListWidgetItem(loadIconFor(tool), QStringLiteral("%1\n%2").arg(tool.name, tool.description));
        item->setData(Qt::UserRole, tool.id);
        item->setToolTip(QStringLiteral("%1\n%2").arg(tool.name, tool.description));
        m_toolList->addItem(item);
    }

    m_summaryLabel->setText(tr("工具：%1").arg(display.size()));
}

void MainWindow::openToolWindow(const ToolDTO &tool)
{
    auto *win = new ToolWindow(m_core, m_toolsRoot, tool, this);
    win->setAttribute(Qt::WA_DeleteOnClose, true);
    win->show();
    win->raise();
    win->activateWindow();
}
