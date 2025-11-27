#pragma once

#include "common/Dto.h"

#include <QMainWindow>
#include <QMap>
#include <QSettings>

class CoreService;
class DynamicForm;
class QListWidget;
class QTextEdit;
class QPushButton;
class QLineEdit;
class QLabel;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(CoreService *core, const QString &toolsRoot, QWidget *parent = nullptr);

private:
    struct AdvOverride
    {
        QString program; // interpreter or executable override
    };

private slots:
    void handleScanFinished(const ScanResultDTO &result);
    void handleJobStarted(const QString &toolId, const QString &runDirectory);
    void handleJobOutput(const QString &toolId, const QString &line, bool isError);
    void handleJobFinished(const QString &toolId, int exitCode, const QString &message);
    void handleEnvPreparing(const QString &toolId);
    void handleEnvFailed(const QString &toolId, const QString &message);
    void handleEnvReady(const QString &toolId, const QString &envPath);
    void handleRunClicked();
    void handleRefreshClicked();
    void handleToolSelectionChanged();
    void handleAdvancedClicked();
    void handleLogMessage(int level, const QString &category, const QString &message);
    void updateAdvSummary(const ToolDTO &tool, const AdvOverride &ov);

private:
    void buildUi();
    void appendLog(const QString &text, bool isError = false);
    void loadTool(const ToolDTO &tool);

    CoreService *m_core{nullptr};
    QString m_toolsRoot;
    QListWidget *m_toolList{nullptr};
    DynamicForm *m_form{nullptr};
    QTextEdit *m_log{nullptr};
    QPushButton *m_runBtn{nullptr};
    QPushButton *m_refreshBtn{nullptr};
    QLineEdit *m_outputDirEdit{nullptr};
    QPushButton *m_advBtn{nullptr};
    QLabel *m_advSummary{nullptr};

    QMap<QString, AdvOverride> m_advOverrides; // keyed by toolId
    QList<ToolDTO> m_tools;

    QSettings m_settings;

    AdvOverride loadOverride(const QString &toolId);
    void saveOverride(const QString &toolId, const AdvOverride &ov);
};
