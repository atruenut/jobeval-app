#include "PackService.hpp"

#include <QtConcurrent>

#include "NetworkFetcher.hpp"

using namespace jobeval;

PackService::PackService(const QString& cacheDir, const QUrl& catalogUrl, const MinisignKey& key, QObject* parent)
    : QObject(parent), store_(cacheDir.toStdString(), key), url_(catalogUrl) {}

bool PackService::loadCachedCatalog() {
    Diagnostics diag;
    Catalog c;
    if (!store_.load_cached_catalog(c, diag)) return false;
    catalog_ = std::move(c);
    has_catalog_ = true;
    return true;
}

bool PackService::loadInstalled(PackSet& set, QString& problems) const {
    Diagnostics diag;
    const bool ok = store_.load_installed(set, diag);
    problems = QString::fromStdString(diag.format());
    return ok;
}

template <typename Fn>
void PackService::start(const QString& status, Fn fn, std::function<void(const Outcome&)> done) {
    busy_ = true;
    Q_EMIT busyChanged(true, status);
    auto* conn = new QMetaObject::Connection;
    *conn = connect(&watcher_, &QFutureWatcher<Outcome>::finished, this, [this, conn, done]() {
        QObject::disconnect(*conn);
        delete conn;
        const Outcome o = watcher_.result();
        busy_ = false;
        done(o);
        Q_EMIT busyChanged(false, QString());
    });
    watcher_.setFuture(QtConcurrent::run(fn));
}

void PackService::refreshAsync() {
    if (busy_) return;
    PackStore store = store_;  // worker gets its own copy; the cache directory is the shared state
    const QUrl url = url_;
    start(tr("Checking for updates…"),
          [store, url]() mutable {
              Outcome o;
              NetworkFetcher fetcher(url);
              Diagnostics diag;
              o.ok = store.refresh_catalog(fetcher, o.catalog, diag);
              if (!o.ok) o.message = QString::fromStdString(diag.format()).trimmed();
              return o;
          },
          [this](const Outcome& o) {
              if (o.ok) {
                  catalog_ = o.catalog;
                  has_catalog_ = true;
              }
              Q_EMIT catalogRefreshed(o.ok, o.message);
          });
}

void PackService::installAsync(const QString& packId) {
    if (busy_ || !has_catalog_) return;
    PackStore store = store_;
    const QUrl url = url_;
    const Catalog catalog = catalog_;
    const std::string id = packId.toStdString();
    start(tr("Downloading and verifying %1…").arg(packId),
          [store, url, catalog, id]() mutable {
              Outcome o;
              NetworkFetcher fetcher(url);
              Diagnostics diag;
              o.ok = store.install(catalog, id, fetcher, diag);
              if (!o.ok) o.message = QString::fromStdString(diag.format()).trimmed();
              return o;
          },
          [this, packId](const Outcome& o) { Q_EMIT installFinished(packId, o.ok, o.message); });
}
