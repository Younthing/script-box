#pragma once

#include "common/Dto.h"

#include <QMainWindow>
#include <QMap>

class CoreService;
class DynamicForm;
class QListWidget;
class QTextEdit;
class QPushButton;
class QLineEdit;
class QLabel;
class QComboBox;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(CoreService *core, const QString &toolsRoot, QWidget *parent = nullptr);

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

    QString m_advInterpreter;
    bool m_hasUvOverride{false};
    bool m_uvOverride{false};

    QMap<QString, ToolDTO> m_tools;
};
