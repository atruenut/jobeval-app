#include "MainWindow.hpp"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QVBoxLayout>

#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <evalcore/pack.hpp>

#include "ComparePage.hpp"
#include "Version.hpp"
#include "EvaluatePage.hpp"
#include "JobPickDialog.hpp"
#include "PackService.hpp"
#include "RegionPage.hpp"

using namespace jobeval;

static const char* kFilter = "Job Evaluator files (*.jobeval);;All files (*)";

MainWindow::MainWindow(PackService* service, const QString& titleSuffix, const QString& sessionFile, QWidget* parent)
    : QMainWindow(parent), service_(service), titleSuffix_(titleSuffix), sessionFile_(sessionFile) {
    resize(1320, 820);
    setMinimumSize(1200, 650);

    stack_ = new QStackedWidget;
    region_ = new RegionPage(service_);
    workspace_ = new QWidget;
    auto* wl = new QHBoxLayout(workspace_);
    wl->setContentsMargins(12, 12, 0, 0);

    auto* side = new QWidget;
    side->setFixedWidth(210);
    auto* sv = new QVBoxLayout(side);
    sv->setContentsMargins(0, 0, 0, 8);
    auto* hdr = new QLabel(tr("Jobs"));
    hdr->setObjectName("sectionTitle");
    sv->addWidget(hdr);
    sort_ = new QComboBox;
    sort_->setToolTip(tr("How the job list is ordered. This only changes the list and the Compare columns, not your project."));
    struct SortItem { const char* label; int id; };
    for (const SortItem& it : {SortItem{QT_TR_NOOP("Order added"), 0}, SortItem{QT_TR_NOOP("Newest first"), 1}, SortItem{QT_TR_NOOP("Name A to Z"), 2},
                               SortItem{QT_TR_NOOP("Name Z to A"), 3}, SortItem{QT_TR_NOOP("Pay, highest first"), 4}, SortItem{QT_TR_NOOP("Pay, lowest first"), 5},
                               SortItem{QT_TR_NOOP("Real hourly, best first"), 6}, SortItem{QT_TR_NOOP("Take-home per month, highest"), 7},
                               SortItem{QT_TR_NOOP("Hours per week, fewest first"), 8}, SortItem{QT_TR_NOOP("Fit score, best first"), 9},
                               SortItem{QT_TR_NOOP("Region"), 10}})
        sort_->addItem(tr(it.label), it.id);
    {
        const int i = sort_->findData(QSettings().value("jobSort", 0).toInt());
        sort_->setCurrentIndex(i >= 0 ? i : 0);
    }
    sv->addWidget(sort_);
    list_ = new QListWidget;
    list_->setObjectName("jobList");
    sv->addWidget(list_, 1);
    auto* add = new QPushButton(tr("+ Add job"));
    add->setObjectName("primary");
    sv->addWidget(add);
    wl->addWidget(side);

    tabs_ = new QTabWidget;
    evaluate_ = new EvaluatePage;
    compare_ = new ComparePage;
    tabs_->addTab(evaluate_, tr("Job"));
    tabs_->addTab(compare_, tr("Compare"));
    wl->addWidget(tabs_, 1);

    stack_->addWidget(region_);
    stack_->addWidget(workspace_);
    setCentralWidget(stack_);

    // ---- menus
    QMenu* file = menuBar()->addMenu(tr("&File"));
    file->addAction(tr("&New project"), QKeySequence::New, this, [this]() { newProject(); });
    file->addAction(tr("&Open project…"), QKeySequence::Open, this, [this]() {
        if (!maybeSave()) return;
        const QString p = QFileDialog::getOpenFileName(this, tr("Open project"), QString(), tr(kFilter));
        if (!p.isEmpty()) openProjectFile(p);
    });
    recentMenu_ = file->addMenu(tr("Open &recent"));
    file->addSeparator();
    actSave_ = file->addAction(tr("&Save project"), QKeySequence::Save, this, [this]() { saveProject(); });
    file->addAction(tr("Save project &as…"), QKeySequence::SaveAs, this, [this]() { saveProjectAs(); });
    file->addSeparator();
    file->addAction(tr("&Import jobs…"), QKeySequence(Qt::CTRL | Qt::Key_I), this, [this]() { importJobs(); });
    actExport_ = file->addAction(tr("&Export jobs…"), QKeySequence(Qt::CTRL | Qt::Key_E), this, [this]() { exportJobs(); });
    file->addSeparator();
    file->addAction(tr("&Quit"), QKeySequence::Quit, this, [this]() { close(); });

    QMenu* jobMenu = menuBar()->addMenu(tr("&Job"));
    jobMenu->addAction(tr("&Add job"), QKeySequence(Qt::CTRL | Qt::Key_T), this, [this]() { addJob(); });
    actDuplicate_ = jobMenu->addAction(tr("&Duplicate job"), QKeySequence(Qt::CTRL | Qt::Key_D), this, [this]() { duplicateJob(); });
    actRename_ = jobMenu->addAction(tr("&Rename job"), QKeySequence(Qt::Key_F2), this, [this]() { renameJob(); });
    actDelete_ = jobMenu->addAction(tr("De&lete job"), QKeySequence::Delete, this, [this]() { deleteJob(); });
    jobMenu->addSeparator();
    actRegion_ = jobMenu->addAction(tr("Change &region…"), this, [this]() { showRegionPage(false); });
    jobMenu->addAction(tr("Check for &rule updates"), this, [this]() { showRegionPage(false); service_->refreshAsync(); });

    QMenu* help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("&About"), this, [this]() {
        QMessageBox::about(this, tr("About Job Evaluator"),
                           tr("<b>Job Evaluator %1</b> (rule engine %2.%3)<br>Works out what a job really pays per hour of your life.<br><br>"
                              "Tax and employment rules come from signed data packs published at "
                              "<a href=\"https://github.com/atruenut/jobeval-data\">github.com/atruenut/jobeval-data</a> (CC BY 4.0); "
                              "the program is MIT licensed: <a href=\"https://github.com/atruenut/jobeval-app\">github.com/atruenut/jobeval-app</a>.<br><br>"
                              "Results are estimates for comparing jobs, not payroll-grade figures. Everything works offline; the network is used only "
                              "to download rule packs and, if you ask, to check for a newer version or fetch exchange rates.")
                               .arg(JOBEVAL_VERSION).arg(jobeval::kEngineMajor).arg(jobeval::kEngineMinor));
    });
    help->addAction(tr("Check for app &updates"), this, [this]() { checkForUpdate(); });
    help->addAction(tr("&User guide online"), this, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/atruenut/jobeval-app/blob/main/docs/USER_GUIDE.md"));
    });

    // ---- wiring
    connect(add, &QPushButton::clicked, this, [this]() { addJob(); });
    connect(sort_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        QSettings().setValue("jobSort", sort_->currentData().toInt());
        rebuildList(currentRow(), false);
        if (tabs_->currentWidget() == compare_) onRowChanged(list_->currentRow());
    });
    connect(list_, &QListWidget::currentRowChanged, this, [this](int r) { onRowChanged(r); });
    connect(list_, &QListWidget::itemDoubleClicked, this, [this]() { renameJob(); });
    connect(tabs_, &QTabWidget::currentChanged, this, [this](int) {
        if (tabs_->currentWidget() == compare_) {
            QMap<QString, QString> names;
            for (const Job& j : project_.jobs) names[j.packId] = regionTitle(j.packId);
            compare_->refresh(sortedJobs(), [this](const QString& id) { return bundleFor(id); }, names, project_.priorities, project_.baselineId, project_.homeCurrency, project_.rates, project_.ratesInfo);
        }
    });
    connect(compare_, &ComparePage::prioritiesChanged, this, [this](const QVariantMap& p) {
        project_.priorities = p;
        markDirty();
        if (sort_->currentData().toInt() == 9) rebuildList(currentRow(), false);
    });
    connect(compare_, &ComparePage::currenciesChanged, this, [this](const QString& home, const QVariantMap& rates, const QString& info) {
        project_.homeCurrency = home;
        project_.ratesInfo = info;
        project_.rates = rates;
        markDirty();
        if (sort_->currentData().toInt() == 9) rebuildList(currentRow(), false);
    });
    connect(compare_, &ComparePage::baselineChanged, this, [this](const QString& id) {
        project_.baselineId = id;
        markDirty();
    });
    connect(region_, &RegionPage::continueRequested, this, [this](const QString& id) { onRegionChosen(id); });
    connect(region_, &RegionPage::cancelRequested, this, [this]() { pendingNewJob_ = false; showWorkspace(); });
    connect(evaluate_, &EvaluatePage::changeRegionRequested, this, [this]() { showRegionPage(false); });
    connect(evaluate_, &EvaluatePage::jobEdited, this, [this](const QString& n, const QString& no, const QVariantMap& s) { onJobEdited(n, no, s); });
    connect(service_, &PackService::catalogRefreshed, this, [this](bool ok, const QString& msg) {
        region_->reload();
        region_->setStatus(msg, !ok);
        statusBar()->showMessage(ok ? tr("Rule catalog up to date") : tr("Could not refresh rules (using saved copy)"), 5000);
    });
    connect(service_, &PackService::installFinished, this, [this](const QString&, bool ok, const QString& msg) {
        invalidateBundles();
        region_->reload();
        region_->setStatus(msg, !ok);
    });
    autosave_.setSingleShot(true);
    autosave_.setInterval(700);
    connect(&autosave_, &QTimer::timeout, this, [this]() { autosaveNow(); });
    rebuildRecentMenu();
    updateActions();
}

// ---------------------------------------------------------------- startup, session

void MainWindow::start(const QString& projectFile) {
    QSettings s;
    if (!projectFile.isEmpty() && openProjectFile(projectFile)) return;
    if (QFileInfo::exists(sessionFile_)) {
        Project p;
        QString err;
        if (readProjectFile(sessionFile_, p, err) && !p.jobs.isEmpty()) {
            setProject(p, s.value("session/path").toString(), s.value("session/dirty", false).toBool());
            return;
        }
    }
    setProject(Project(), QString(), false);
    showRegionPage(true);
}

void MainWindow::autosaveNow() {
    QString err;
    QDir().mkpath(QFileInfo(sessionFile_).absolutePath());
    writeProjectFile(sessionFile_, project_, "project", err);
    QSettings s;
    s.setValue("session/path", path_);
    s.setValue("session/dirty", dirty_);
}

void MainWindow::markDirty() {
    dirty_ = true;
    updateTitle();
    autosave_.start();
}

void MainWindow::updateTitle() {
    const QString name = !path_.isEmpty() ? QFileInfo(path_).completeBaseName() : (project_.name.isEmpty() ? tr("Untitled project") : project_.name);
    setWindowTitle(QString("%1%2 – Job Evaluator %3%4").arg(name, dirty_ ? "*" : "", JOBEVAL_VERSION, titleSuffix_));
}

void MainWindow::setProject(const Project& p, const QString& path, bool dirty) {
    project_ = p;
    path_ = path;
    dirty_ = dirty;
    invalidateBundles();
    refreshList(project_.jobs.isEmpty() ? -1 : 0);
    updateTitle();
    if (project_.jobs.isEmpty()) {
        showWorkspace();
    } else {
        showWorkspace();
    }
    updateActions();
}

bool MainWindow::openProjectFile(const QString& path) {
    Project p;
    QString err;
    if (!readProjectFile(path, p, err)) {
        QMessageBox::warning(this, tr("Cannot open project"), err);
        return false;
    }
    if (p.name.isEmpty()) p.name = QFileInfo(path).completeBaseName();
    setProject(p, path, false);
    addRecent(path);
    autosaveNow();
    return true;
}

bool MainWindow::saveProject() {
    if (path_.isEmpty()) return saveProjectAs();
    QString err;
    if (!writeProjectFile(path_, project_, "project", err)) {
        QMessageBox::warning(this, tr("Cannot save"), err);
        return false;
    }
    dirty_ = false;
    updateTitle();
    addRecent(path_);
    autosaveNow();
    statusBar()->showMessage(tr("Saved %1").arg(path_), 4000);
    return true;
}

bool MainWindow::saveProjectAs() {
    QString suggested = path_.isEmpty() ? (project_.name.isEmpty() ? tr("My jobs") : project_.name) + ".jobeval" : path_;
    QString p = QFileDialog::getSaveFileName(this, tr("Save project"), suggested, tr(kFilter));
    if (p.isEmpty()) return false;
    if (QFileInfo(p).suffix().isEmpty()) p += ".jobeval";
    path_ = p;
    project_.name = QFileInfo(p).completeBaseName();
    return saveProject();
}

bool MainWindow::maybeSave() {
    if (!dirty_) return true;
    const auto r = QMessageBox::question(this, tr("Unsaved changes"),
                                         tr("Save changes to this project before continuing?\n(Your work is also kept automatically and restored next time.)"),
                                         QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (r == QMessageBox::Cancel) return false;
    if (r == QMessageBox::Save) return saveProject();
    return true;
}

void MainWindow::closeEvent(QCloseEvent* e) {
    if (!maybeSave()) {
        e->ignore();
        return;
    }
    autosaveNow();
    e->accept();
}

void MainWindow::newProject() {
    if (!maybeSave()) return;
    setProject(Project(), QString(), false);
    autosaveNow();
    showRegionPage(true);
}

void MainWindow::addRecent(const QString& path) {
    QSettings s;
    QStringList r = s.value("recent").toStringList();
    r.removeAll(path);
    r.prepend(path);
    while (r.size() > 8) r.removeLast();
    s.setValue("recent", r);
    rebuildRecentMenu();
}

void MainWindow::rebuildRecentMenu() {
    recentMenu_->clear();
    const QStringList r = QSettings().value("recent").toStringList();
    for (const QString& p : r) {
        if (!QFileInfo::exists(p)) continue;
        recentMenu_->addAction(QFileInfo(p).fileName(), this, [this, p]() {
            if (maybeSave()) openProjectFile(p);
        });
    }
    recentMenu_->setEnabled(!recentMenu_->isEmpty());
}

void MainWindow::importJobs() {
    const QString p = QFileDialog::getOpenFileName(this, tr("Import jobs"), QString(), tr(kFilter));
    if (p.isEmpty()) return;
    Project in;
    QString err;
    if (!readProjectFile(p, in, err)) {
        QMessageBox::warning(this, tr("Cannot import"), err);
        return;
    }
    if (in.jobs.isEmpty()) {
        QMessageBox::information(this, tr("Import jobs"), tr("That file contains no jobs."));
        return;
    }
    JobPickDialog dlg(tr("Import jobs"), tr("Choose the jobs to add to this project:"), in.jobs, this);
    if (dlg.exec() != QDialog::Accepted) return;
    const QList<int> pick = dlg.selected();
    if (pick.isEmpty()) return;
    QStringList ids;
    for (const Job& j : project_.jobs) ids << j.id;
    int firstNew = static_cast<int>(project_.jobs.size());
    for (int i : pick) {
        Job j = in.jobs[i];
        if (ids.contains(j.id)) j.id = newJobId();
        ids << j.id;
        project_.jobs.append(j);
    }
    markDirty();
    refreshList(firstNew);
    showWorkspace();
    updateActions();
    statusBar()->showMessage(tr("Imported %1 job(s)").arg(pick.size()), 5000);
}

void MainWindow::exportJobs() {
    if (project_.jobs.isEmpty()) return;
    JobPickDialog dlg(tr("Export jobs"), tr("Choose the jobs to export. The file can be imported into any project:"), project_.jobs, this);
    if (dlg.exec() != QDialog::Accepted) return;
    const QList<int> pick = dlg.selected();
    if (pick.isEmpty()) return;
    QString p = QFileDialog::getSaveFileName(this, tr("Export jobs"), pick.size() == 1 ? project_.jobs[pick[0]].name + ".jobeval" : tr("Exported jobs.jobeval"), tr(kFilter));
    if (p.isEmpty()) return;
    if (QFileInfo(p).suffix().isEmpty()) p += ".jobeval";
    Project out;
    out.name = project_.name;
    for (int i : pick) out.jobs.append(project_.jobs[i]);
    QString err;
    if (!writeProjectFile(p, out, "jobs", err)) {
        QMessageBox::warning(this, tr("Cannot export"), err);
        return;
    }
    statusBar()->showMessage(tr("Exported %1 job(s) to %2").arg(pick.size()).arg(p), 5000);
}

// ---------------------------------------------------------------- jobs

int MainWindow::currentRow() const {
    const int r = list_->currentRow();
    return r >= 0 && r < order_.size() ? order_[r] : -1;
}

int MainWindow::listRowOf(int projectIndex) const { return order_.indexOf(projectIndex); }

QList<Job> MainWindow::sortedJobs() const {
    QList<Job> out;
    if (order_.size() != project_.jobs.size()) return project_.jobs;
    for (int i : order_) out.append(project_.jobs[i]);
    return out;
}

Job MainWindow::makeJob(const QString& packId) const {
    Job j;
    j.id = newJobId();
    j.packId = packId;
    j.state = defaultState();
    int n = static_cast<int>(project_.jobs.size()) + 1;
    QStringList names;
    for (const Job& e : project_.jobs) names << e.name;
    QString name;
    do name = tr("Job %1").arg(n++);
    while (names.contains(name));
    j.name = name;
    return j;
}

void MainWindow::addJob() {
    if (project_.jobs.isEmpty()) {
        showRegionPage(true);
        return;
    }
    const int r = qMax(0, currentRow());
    Job j = makeJob(project_.jobs[r].packId);
    project_.jobs.append(j);
    markDirty();
    refreshList(static_cast<int>(project_.jobs.size()) - 1);
    tabs_->setCurrentWidget(evaluate_);
    updateActions();
}

void MainWindow::duplicateJob() {
    const int r = currentRow();
    if (r < 0) return;
    Job j = project_.jobs[r];
    j.id = newJobId();
    j.name = tr("%1 (copy)").arg(j.name);
    project_.jobs.insert(r + 1, j);
    markDirty();
    refreshList(r + 1);
    updateActions();
}

void MainWindow::deleteJob() {
    const int r = currentRow();
    if (r < 0) return;
    if (QMessageBox::question(this, tr("Delete job"), tr("Delete “%1” from this project?").arg(project_.jobs[r].name)) != QMessageBox::Yes) return;
    project_.jobs.removeAt(r);
    markDirty();
    refreshList(qMin(r, static_cast<int>(project_.jobs.size()) - 1));
    updateActions();
}

void MainWindow::renameJob() {
    const int r = currentRow();
    if (r < 0) return;
    bool ok = false;
    const QString n = QInputDialog::getText(this, tr("Rename job"), tr("Job name:"), QLineEdit::Normal, project_.jobs[r].name, &ok);
    if (!ok || n.trimmed().isEmpty()) return;
    project_.jobs[r].name = n.trimmed();
    markDirty();
    updateListItem(r);
    evaluate_->setJobName(project_.jobs[r].name);
}

void MainWindow::refreshList(int select) { rebuildList(select, true); }

void MainWindow::rebuildList(int select, bool notify) {
    const int n = static_cast<int>(project_.jobs.size());
    auto lookup = [this](const QString& id) { return bundleFor(id); };
    QList<JobResult> results;
    for (const Job& j : project_.jobs) results.append(evaluateJob(j, lookup, QString()));
    applyRates(results, project_.homeCurrency, project_.rates);
    scoreResults(results, project_.priorities);

    // Jobs without a value for the chosen key (not-working entries for pay, failures) go last.
    order_ = sortOrder(results, sort_->currentData().toInt(), [this](const QString& id) { return regionTitle(id); });

    QSignalBlocker blocker(list_);
    list_->clear();
    for (int i : order_) {
        const JobResult& r = results[i];
        QString sub = tr("rules not downloaded");
        if (!r.currency.isEmpty()) {
            if (!r.ok) sub = tr("check inputs");
            else if (r.notWorking) sub = tr("not working, %1 %2/mo").arg(QLocale().toString(r.monthlyValue, 'f', 0), r.currency);
            else sub = r.hasHourly ? tr("%1 %2 real hourly").arg(QString::number(r.realHourly, 'f', 2), r.currency) : QStringLiteral("–");
        }
        list_->addItem(project_.jobs[i].name + "\n" + sub);
    }
    blocker.unblock();
    const int row = listRowOf(select);
    QSignalBlocker again(list_);
    list_->setCurrentRow(row);
    again.unblock();
    if (notify) onRowChanged(row);
}

void MainWindow::updateListItem(int) { rebuildList(currentRow(), false); }

void MainWindow::onRowChanged(int listRow) {
    const int row = listRow >= 0 && listRow < order_.size() ? order_[listRow] : -1;
    const bool has = row >= 0 && row < project_.jobs.size();
    tabs_->setTabEnabled(0, has);
    if (!has) {
        tabs_->setCurrentWidget(compare_);
        compare_->refresh(sortedJobs(), [this](const QString& id) { return bundleFor(id); }, {}, project_.priorities, project_.baselineId, project_.homeCurrency, project_.rates, project_.ratesInfo);
        return;
    }
    const Job& j = project_.jobs[row];
    const CatalogEntry* e = service_->hasCatalog() ? service_->catalog().find(j.packId.toStdString()) : nullptr;
    evaluate_->setJob(j, bundleFor(j.packId), e, regionTitle(j.packId));
    if (tabs_->currentWidget() == compare_) {
        QMap<QString, QString> names;
        for (const Job& jj : project_.jobs) names[jj.packId] = regionTitle(jj.packId);
        compare_->refresh(sortedJobs(), [this](const QString& id) { return bundleFor(id); }, names, project_.priorities, project_.baselineId, project_.homeCurrency, project_.rates, project_.ratesInfo);
    }
    updateActions();
}

void MainWindow::onJobEdited(const QString& name, const QString& notes, const QVariantMap& state) {
    const int r = currentRow();
    if (r < 0 || r >= project_.jobs.size()) return;
    Job& j = project_.jobs[r];
    j.name = name.isEmpty() ? j.name : name;
    j.notes = notes;
    j.state = state;
    updateListItem(r);
    markDirty();
}

void MainWindow::updateActions() {
    const bool has = !project_.jobs.isEmpty();
    for (QAction* a : {actExport_, actDuplicate_, actDelete_, actRename_, actRegion_}) a->setEnabled(has);
}

// ---------------------------------------------------------------- regions

void MainWindow::invalidateBundles() {
    packsLoaded_ = false;
    packSet_ = PackSet();
    bundleCache_.clear();
}

std::shared_ptr<const Bundle> MainWindow::bundleFor(const QString& packId) {
    if (packId.isEmpty()) return nullptr;
    if (bundleCache_.contains(packId)) return bundleCache_[packId];
    if (!packsLoaded_) {
        QString problems;
        service_->loadInstalled(packSet_, problems);
        packsLoaded_ = true;
    }
    Diagnostics diag;
    auto b = packSet_.bundle_for(packId.toStdString(), diag);
    bundleCache_[packId] = b;
    return b;
}

QString MainWindow::regionTitle(const QString& packId) const {
    if (!service_->hasCatalog()) return packId;
    const CatalogEntry* e = service_->catalog().find(packId.toStdString());
    if (!e) return packId;
    auto label = [](const CatalogEntry& c) {
        auto it = c.labels.find("en");
        return QString::fromStdString(it != c.labels.end() ? it->second : c.id);
    };
    QString t = label(*e);
    if (!e->extends.empty())
        if (const CatalogEntry* parent = service_->catalog().find(e->extends)) t += ", " + label(*parent);
    return t;
}

void MainWindow::showRegionPage(bool forNewJob) {
    pendingNewJob_ = forNewJob;
    region_->setCancelable(!project_.jobs.isEmpty());
    const int r = currentRow();
    if (!forNewJob && r >= 0 && r < project_.jobs.size()) region_->selectPack(project_.jobs[r].packId);
    stack_->setCurrentWidget(region_);
}

void MainWindow::showWorkspace() { stack_->setCurrentWidget(workspace_); }

void MainWindow::onRegionChosen(const QString& packId) {
    if (packId.isEmpty()) return;
    if (pendingNewJob_) {
        Job j = makeJob(packId);
        project_.jobs.append(j);
        pendingNewJob_ = false;
        markDirty();
        refreshList(static_cast<int>(project_.jobs.size()) - 1);
        tabs_->setCurrentWidget(evaluate_);
    } else {
        const int r = currentRow();
        if (r >= 0 && r < project_.jobs.size()) {
            project_.jobs[r].packId = packId;
            markDirty();
            updateListItem(r);
            onRowChanged(r);
        }
    }
    invalidateBundles();
    for (int i = 0; i < project_.jobs.size(); ++i) updateListItem(i);
    showWorkspace();
    updateActions();
}

// ---------------------------------------------------------------- developer helpers

void MainWindow::applyStateToCurrent(const QVariantMap& values) {
    const int r = currentRow();
    if (r < 0) return;
    QVariantMap st = project_.jobs[r].state;
    for (auto it = values.begin(); it != values.end(); ++it) st[it.key()] = it.value();
    project_.jobs[r].state = normalizedState(st);
    onRowChanged(r);
    updateListItem(r);
}

void MainWindow::showCompareTab() { tabs_->setCurrentWidget(compare_); }

void MainWindow::selectJobRow(int row) { list_->setCurrentRow(listRowOf(row)); }

void MainWindow::addJobInRegion(const QString& packId) {
    pendingNewJob_ = true;
    onRegionChosen(packId);
}

void MainWindow::showCompareSubTab(int sub, int chart) {
    showCompareTab();
    compare_->showSubTab(sub, chart);
}

void MainWindow::setSortMode(int id) {
    const int i = sort_->findData(id);
    if (i >= 0) sort_->setCurrentIndex(i);
}

void MainWindow::setCompareScenario(const QVariantMap& v) { compare_->setScenario(v); }

void MainWindow::checkForUpdate() {
    if (!nam_) nam_ = new QNetworkAccessManager(this);
    QNetworkRequest req(QUrl("https://api.github.com/repos/atruenut/jobeval-app/releases/latest"));
    req.setTransferTimeout(15000);
    req.setHeader(QNetworkRequest::UserAgentHeader, "jobeval/" JOBEVAL_VERSION);
    QNetworkReply* reply = nam_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::information(this, tr("Check for updates"), tr("Could not reach GitHub (%1). Nothing has changed.").arg(reply->errorString()));
            return;
        }
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll().left(1 << 20)).object();
        const QString tag = o["tag_name"].toString();
        if (tag.isEmpty()) {
            QMessageBox::information(this, tr("Check for updates"), tr("No published release was found."));
        } else if (compareVersions(tag, JOBEVAL_VERSION) > 0) {
            if (QMessageBox::question(this, tr("Check for updates"), tr("Version %1 is available (you have %2). Open the download page?").arg(tag, JOBEVAL_VERSION)) == QMessageBox::Yes)
                QDesktopServices::openUrl(QUrl(o["html_url"].toString().startsWith("https://github.com/") ? o["html_url"].toString() : QStringLiteral("https://github.com/atruenut/jobeval-app/releases")));
        } else {
            QMessageBox::information(this, tr("Check for updates"), tr("You have the latest version (%1).").arg(JOBEVAL_VERSION));
        }
    });
}
