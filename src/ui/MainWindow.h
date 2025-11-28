#pragma once

#pragma once

#include "common/Dto.h"

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QVersionNumber>
#include <QList>
#include <QString>

class CoreService;
class QListWidget;
class QPushButton;
class QLabel;
class QLineEdit;
class QListWidgetItem;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(CoreService *core, const QString &toolsRoot, QWidget *parent = nullptr);

private slots:
    void handleScanFinished(const ScanResultDTO &result);
    void handleRefreshClicked();
    void handleCategoryChanged();
    void handleToolActivated(QListWidgetItem *item);
    void handleToggleView();
    void handleUpdateClicked();

private:
    void buildUi();
    void rebuildCategories();
    void rebuildToolList();
    QIcon loadIconFor(const ToolDTO &tool) const;
    QList<ToolDTO> filteredTools() const;
    void openToolWindow(const ToolDTO &tool);
    void checkForUpdates(bool manual);
    void downloadUpdate(const QUrl &url, const QVersionNumber &remoteVersion);
    bool launchUpdater(const QString &zipPath, const QString &logPath);
    QString resolveUpdateUrl() const;
    QString ensureUpdateWorkDir() const;
    QString psEscape(const QString &path) const;
    void resetUpdateButton();

    struct UpdateMeta
    {
        QVersionNumber version;
        QUrl url;
        QString notes;
        bool valid() const { return version.isNormalized() && url.isValid(); }
    };

    CoreService *m_core{nullptr};
    QString m_toolsRoot;

    QListWidget *m_categoryList{nullptr};
    QListWidget *m_toolList{nullptr};
    QPushButton *m_refreshBtn{nullptr};
    QPushButton *m_toggleViewBtn{nullptr};
    QPushButton *m_updateBtn{nullptr};
    QLabel *m_summaryLabel{nullptr};

    QList<ToolDTO> m_tools;
    bool m_cardMode{true};
    QNetworkAccessManager m_network;
    UpdateMeta m_latestMeta;
};
