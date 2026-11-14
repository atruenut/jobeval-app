#pragma once

#include <QMainWindow>
#include <QMap>
#include <QTimer>
#include <QVariantMap>
#include <memory>

#include "JobModel.hpp"
#include "evalcore/pack.hpp"

class ComparePage;
class EvaluatePage;
class PackService;
class QAction;
class QComboBox;
class QListWidget;
class QMenu;
class QNetworkAccessManager;
class QStackedWidget;
class QTabWidget;
class RegionPage;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(PackService* service, const QString& titleSuffix, const QString& sessionFile, QWidget* parent = nullptr);

    // Startup: restores the previous session (or opens `projectFile` when given).
    void start(const QString& projectFile);
    // Developer/testing helpers.
    void applyStateToCurrent(const QVariantMap& values);
    void showCompareTab();
    void showCompareSubTab(int sub, int chart);
    void setCompareScenario(const QVariantMap& v);
    void selectJobRow(int row);
    void setSortMode(int id);
    void addJobInRegion(const QString& packId);
    void showRegionPage(bool forNewJob);
    int jobCount() const { return static_cast<int>(project_.jobs.size()); }

protected:
    void closeEvent(QCloseEvent* e) override;

private:
    // project file handling
    bool openProjectFile(const QString& path);
    bool saveProject();
    bool saveProjectAs();
    bool maybeSave();
    void newProject();
    void importJobs();
    void exportJobs();
    void setProject(const Project& p, const QString& path, bool dirty);
    void markDirty();
    void autosaveNow();
    void updateTitle();
    void addRecent(const QString& path);
    void rebuildRecentMenu();

    // jobs
    void addJob();
    void duplicateJob();
    void deleteJob();
    void renameJob();
    void checkForUpdate();
    QNetworkAccessManager* nam_ = nullptr;
    Job makeJob(const QString& packId) const;
    void refreshList(int select);
    void rebuildList(int select, bool notify);   // select is a project index; notify = load it into the editor
    void updateListItem(int row);                // row is a project index
    QList<Job> sortedJobs() const;
    int listRowOf(int projectIndex) const;
    int currentRow() const;
    void onRowChanged(int row);
    void onJobEdited(const QString& name, const QString& notes, const QVariantMap& state);

    // regions and rules
    std::shared_ptr<const jobeval::Bundle> bundleFor(const QString& packId);
    QString regionTitle(const QString& packId) const;
    void invalidateBundles();
    void showWorkspace();
    void onRegionChosen(const QString& packId);
    void updateActions();

    PackService* service_;
    QString titleSuffix_;
    QString sessionFile_;
    Project project_;
    QString path_;
    bool dirty_ = false;
    bool pendingNewJob_ = false;
    bool packsLoaded_ = false;
    jobeval::PackSet packSet_;
    QMap<QString, std::shared_ptr<const jobeval::Bundle>> bundleCache_;
    QTimer autosave_;

    QStackedWidget* stack_;
    RegionPage* region_;
    QWidget* workspace_;
    QListWidget* list_;
    QComboBox* sort_;
    QVector<int> order_;  // list row -> project index (the list can be sorted; the project keeps its order)
    QTabWidget* tabs_;
    EvaluatePage* evaluate_;
    ComparePage* compare_;
    QMenu* recentMenu_;
    QAction* actSave_;
    QAction* actExport_;
    QAction* actDuplicate_;
    QAction* actDelete_;
    QAction* actRename_;
    QAction* actRegion_;
};
