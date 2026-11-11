#pragma once

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QUrl>

#include "evalcore/packstore.hpp"

// Runs catalog refreshes and pack installs on a worker thread and reports back on the GUI
// thread. Only one operation runs at a time.
class PackService : public QObject {
    Q_OBJECT
public:
    struct Outcome {
        bool ok = false;
        QString message;
        jobeval::Catalog catalog;
    };

    PackService(const QString& cacheDir, const QUrl& catalogUrl, const jobeval::MinisignKey& key, QObject* parent = nullptr);

    bool busy() const { return busy_; }
    bool hasCatalog() const { return has_catalog_; }
    const jobeval::Catalog& catalog() const { return catalog_; }
    // Loads the last verified catalog from disk (works offline).
    bool loadCachedCatalog();

    void refreshAsync();
    void installAsync(const QString& packId);

    // Reads of the on-disk state; only call when not busy().
    std::map<std::string, jobeval::InstallRecord> installed() const { return store_.installed(); }
    bool loadInstalled(jobeval::PackSet& set, QString& problems) const;

Q_SIGNALS:
    void busyChanged(bool busy, const QString& status);
    void catalogRefreshed(bool ok, const QString& message);
    void installFinished(const QString& packId, bool ok, const QString& message);

private:
    template <typename Fn>
    void start(const QString& status, Fn fn, std::function<void(const Outcome&)> done);

    jobeval::PackStore store_;
    QUrl url_;
    jobeval::Catalog catalog_;
    bool has_catalog_ = false;
    bool busy_ = false;
    QFutureWatcher<Outcome> watcher_;
};
