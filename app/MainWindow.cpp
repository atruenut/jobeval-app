#include "MainWindow.hpp"

#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>

#include "EvaluatePage.hpp"
#include "PackService.hpp"
#include "RegionPage.hpp"

using namespace jobeval;

MainWindow::MainWindow(PackService* service, const QString& titleSuffix, QWidget* parent)
    : QMainWindow(parent), service_(service) {
    setWindowTitle(QString("Job Evaluator %1%2").arg(JOBEVAL_VERSION, titleSuffix));
    resize(1180, 780);
    setMinimumSize(980, 640);

    stack_ = new QStackedWidget;
    region_ = new RegionPage(service_);
    evaluate_ = new EvaluatePage;
    stack_->addWidget(region_);
    stack_->addWidget(evaluate_);
    setCentralWidget(stack_);

    connect(region_, &RegionPage::continueRequested, this, [this](const QString& id) { openRegion(id); });
    connect(evaluate_, &EvaluatePage::changeRegionRequested, this, &MainWindow::showRegionPage);
    connect(evaluate_, &EvaluatePage::inputsChanged, this, [this]() { saveSettings(); });

    connect(service_, &PackService::catalogRefreshed, this, [this](bool ok, const QString& msg) {
        region_->reload();
        region_->setStatus(msg, !ok);
        statusBar()->showMessage(ok ? tr("Rule catalog up to date") : tr("Could not refresh rules (using saved copy)"), 5000);
    });
    connect(service_, &PackService::installFinished, this, [this](const QString&, bool ok, const QString& msg) {
        region_->reload();
        region_->setStatus(msg, !ok);
    });

    QSettings s;
    const QString last = s.value("region/last").toString();
    evaluate_->setState(s.value("job/state").toMap());
    if (service_->hasCatalog() && !last.isEmpty()) {
        region_->selectPack(last);
        if (!openRegion(last)) showRegionPage();
    }
}

void MainWindow::applyState(const QVariantMap& values) { evaluate_->setState(values); }

void MainWindow::showRegionPage() { stack_->setCurrentWidget(region_); }

bool MainWindow::openRegion(const QString& packId) {
    PackSet set;
    QString problems;
    if (!service_->loadInstalled(set, problems)) {
        region_->setStatus(tr("Saved rules could not be loaded: %1").arg(problems), true);
        return false;
    }
    Diagnostics diag;
    auto bundle = set.bundle_for(packId.toStdString(), diag);
    if (!bundle) {
        region_->setStatus(tr("Rules for this region are not usable: %1").arg(QString::fromStdString(diag.format())), true);
        return false;
    }
    const CatalogEntry* e = service_->hasCatalog() ? service_->catalog().find(packId.toStdString()) : nullptr;
    QString title = packId;
    if (e) {
        auto it = e->labels.find("en");
        if (it != e->labels.end()) title = QString::fromStdString(it->second);
        if (!e->extends.empty()) {
            const CatalogEntry* parent = service_->catalog().find(e->extends);
            if (parent) {
                auto pit = parent->labels.find("en");
                if (pit != parent->labels.end()) title += ", " + QString::fromStdString(pit->second);
            }
        }
    }
    currentPack_ = packId;
    evaluate_->setRegion(bundle, e, title);
    stack_->setCurrentWidget(evaluate_);
    saveSettings();
    return true;
}

void MainWindow::saveSettings() const {
    QSettings s;
    if (!currentPack_.isEmpty()) s.setValue("region/last", currentPack_);
    s.setValue("job/state", evaluate_->state());
}
