#pragma once

#include "common/Dto.h"

#include <QMainWindow>
#include <QMap>
#include <QSettings>

class CoreService;
class DynamicForm;
class QTextEdit;
class QPushButton;
class QLineEdit;
class QLabel;

class ToolWindow : public QMainWindow
{
    Q_OBJECT
public:
    ToolWindow(CoreService *core, const QString &toolsRoot, const ToolDTO &tool, QWidget *parent = nullptr);

private slots:
    void handleRunClicked();
    void handleAdvancedClicked();
    void handleJobStarted(const QString &toolId, const QString &runDirectory);
    void handleJobOutput(const QString &toolId, const QString &line, bool isError);
    void handleJobFinished(const QString &toolId, int exitCode, const QString &message);
    void handleEnvPreparing(const QString &toolId);
    void handleEnvFailed(const QString &toolId, const QString &message);
    void handleEnvReady(const QString &toolId, const QString &envPath);

private:
    struct AdvOverride
    {
        QString program;
    };

    void buildUi();
    void appendLog(const QString &text, bool isError = false);
    void updateAdvSummary(const AdvOverride &ov);
    AdvOverride loadOverride();
    void saveOverride(const AdvOverride &ov);

    CoreService *m_core{nullptr};
    QString m_toolsRoot;
    ToolDTO m_tool;

    DynamicForm *m_form{nullptr};
    QTextEdit *m_log{nullptr};
    QPushButton *m_runBtn{nullptr};
    QPushButton *m_advBtn{nullptr};
    QLabel *m_advSummary{nullptr};
    QLineEdit *m_outputDirEdit{nullptr};

    AdvOverride m_override;
    QSettings m_settings;
};
