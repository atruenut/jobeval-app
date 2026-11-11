#include "RegionPage.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

#include "PackService.hpp"

using namespace jobeval;

namespace {

QString labelOf(const CatalogEntry& e) {
    auto it = e.labels.find("en");
    return QString::fromStdString(it != e.labels.end() ? it->second : e.id);
}

QString tierText(const std::string& tier) {
    if (tier == "full") return QObject::tr("Full coverage");
    if (tier == "partial") return QObject::tr("Partial coverage");
    return QObject::tr("Rough estimate");
}

}  // namespace

RegionPage::RegionPage(PackService* service, QWidget* parent) : QWidget(parent), service_(service) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(32, 28, 32, 24);
    root->setSpacing(14);

    auto* title = new QLabel(tr("Where do you work?"));
    title->setObjectName("pageTitle");
    auto* intro = new QLabel(tr("Tax, contribution and employment rules differ by region. They are downloaded as "
                                "verified data packs, kept on this computer, and work offline afterwards."));
    intro->setWordWrap(true);
    intro->setObjectName("muted");
    root->addWidget(title);
    root->addWidget(intro);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft);
    form->setSpacing(10);
    country_ = new QComboBox;
    subregion_ = new QComboBox;
    country_->setMinimumWidth(280);
    subregion_->setMinimumWidth(280);
    form->addRow(tr("Country"), country_);
    form->addRow(tr("Province / state"), subregion_);
    root->addLayout(form);

    coverage_ = new QLabel;
    coverage_->setWordWrap(true);
    installState_ = new QLabel;
    installState_->setWordWrap(true);
    root->addWidget(coverage_);
    root->addWidget(installState_);

    auto* buttons = new QHBoxLayout;
    download_ = new QPushButton(tr("Download rules"));
    refresh_ = new QPushButton(tr("Check for updates"));
    buttons->addWidget(download_);
    buttons->addWidget(refresh_);
    buttons->addStretch(1);
    root->addLayout(buttons);

    progress_ = new QProgressBar;
    progress_->setRange(0, 0);
    progress_->setTextVisible(false);
    progress_->setMaximumHeight(6);
    progress_->hide();
    root->addWidget(progress_);

    message_ = new QLabel;
    message_->setWordWrap(true);
    message_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(message_);

    root->addStretch(1);
    auto* bottom = new QHBoxLayout;
    bottom->addStretch(1);
    go_ = new QPushButton(tr("Continue"));
    go_->setObjectName("primary");
    go_->setDefault(true);
    bottom->addWidget(go_);
    root->addLayout(bottom);

    connect(country_, &QComboBox::currentIndexChanged, this, [this]() {
        populateSubregions();
        updateState();
    });
    connect(subregion_, &QComboBox::currentIndexChanged, this, [this]() { updateState(); });
    connect(download_, &QPushButton::clicked, this, [this]() {
        setStatus(QString());
        service_->installAsync(selectedPackId());
    });
    connect(refresh_, &QPushButton::clicked, this, [this]() {
        setStatus(QString());
        service_->refreshAsync();
    });
    connect(go_, &QPushButton::clicked, this, [this]() { Q_EMIT continueRequested(selectedPackId()); });
    connect(service_, &PackService::busyChanged, this, [this](bool busy, const QString& status) {
        progress_->setVisible(busy);
        if (busy) setStatus(status);
        updateState();
    });
    reload();
}

const CatalogEntry* RegionPage::entryFor(const QString& id) const {
    return service_->hasCatalog() ? service_->catalog().find(id.toStdString()) : nullptr;
}

QString RegionPage::selectedPackId() const {
    const QVariant v = subregion_->currentData();
    return v.isValid() ? v.toString() : QString();
}

void RegionPage::populateSubregions() {
    subregion_->blockSignals(true);
    subregion_->clear();
    const QString countryId = country_->currentData().toString();
    if (!countryId.isEmpty()) {
        subregion_->addItem(tr("Whole country"), countryId);
        for (const auto& e : service_->catalog().packs) {
            if (e.extends == countryId.toStdString()) subregion_->addItem(labelOf(e), QString::fromStdString(e.id));
        }
    }
    subregion_->setEnabled(subregion_->count() > 1);
    subregion_->blockSignals(false);
}

void RegionPage::reload() {
    const QString keep = selectedPackId();
    country_->blockSignals(true);
    country_->clear();
    if (service_->hasCatalog()) {
        for (const auto& e : service_->catalog().packs) {
            if (e.kind == "country") country_->addItem(labelOf(e), QString::fromStdString(e.id));
        }
    }
    country_->blockSignals(false);
    populateSubregions();
    if (!keep.isEmpty()) selectPack(keep);
    updateState();
}

void RegionPage::selectPack(const QString& packId) {
    const CatalogEntry* e = entryFor(packId);
    if (!e) return;
    const QString countryId = QString::fromStdString(e->extends.empty() ? e->id : e->extends);
    int ci = country_->findData(countryId);
    if (ci < 0) return;
    country_->blockSignals(true);
    country_->setCurrentIndex(ci);
    country_->blockSignals(false);
    populateSubregions();
    int si = subregion_->findData(packId);
    if (si >= 0) subregion_->setCurrentIndex(si);
    updateState();
}

void RegionPage::setStatus(const QString& text, bool error) {
    message_->setText(text);
    message_->setObjectName(error ? "error" : "muted");
    message_->style()->unpolish(message_);
    message_->style()->polish(message_);
}

void RegionPage::updateState() {
    const bool busy = service_->busy();
    const bool haveCatalog = service_->hasCatalog();
    const QString id = selectedPackId();
    const CatalogEntry* e = entryFor(id);

    refresh_->setEnabled(!busy);
    country_->setEnabled(!busy && country_->count() > 0);
    subregion_->setEnabled(!busy && subregion_->count() > 1);

    if (!haveCatalog) {
        coverage_->setText(tr("No rule catalog has been downloaded yet. Connect to the internet and choose "
                              "“Check for updates”."));
        installState_->clear();
        download_->setEnabled(false);
        go_->setEnabled(false);
        return;
    }
    if (!e) {
        coverage_->clear();
        installState_->clear();
        download_->setEnabled(false);
        go_->setEnabled(false);
        return;
    }
    coverage_->setText(tr("<b>%1</b>. Rules last verified %2. Valid %3 to %4.")
                           .arg(tierText(e->tier), QString::fromStdString(e->last_verified.empty() ? "unknown" : e->last_verified),
                                QString::fromStdString(e->valid_from),
                                QString::fromStdString(e->valid_to.empty() ? "open-ended" : e->valid_to)));

    // Installed state covers the pack and everything it extends.
    std::vector<const CatalogEntry*> plan;
    Diagnostics diag;
    const auto installed = busy ? std::map<std::string, InstallRecord>() : service_->installed();
    const bool planned = plan_install(service_->catalog(), id.toStdString(), installed, plan, diag);
    if (!planned) {
        installState_->setText(tr("<span style='color:#b3261e'>This region needs a newer version of the app: %1</span>")
                                   .arg(QString::fromStdString(diag.format()).trimmed().toHtmlEscaped()));
        download_->setEnabled(false);
        go_->setEnabled(false);
        return;
    }
    const bool present = installed.count(id.toStdString()) > 0;
    if (plan.empty()) {
        installState_->setText(tr("Downloaded and up to date."));
        download_->setText(tr("Download rules"));
        download_->setEnabled(false);
    } else if (present) {
        installState_->setText(tr("A newer revision of these rules is available."));
        download_->setText(tr("Update rules"));
        download_->setEnabled(!busy);
    } else {
        installState_->setText(tr("Not downloaded yet (%1 file(s)).").arg(plan.size()));
        download_->setText(tr("Download rules"));
        download_->setEnabled(!busy);
    }
    go_->setEnabled(!busy && present);
}
