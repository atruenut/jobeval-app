#pragma once

#include <QUrl>
#include <memory>

#include "evalcore/packstore.hpp"

class QNetworkAccessManager;

// Blocking HTTP(S)/file fetcher for jobeval::PackStore. Must be used from a worker thread:
// it runs a local event loop while a request is in flight.
class NetworkFetcher : public jobeval::Fetcher {
public:
    explicit NetworkFetcher(QUrl base);
    ~NetworkFetcher() override;
    bool get(const std::string& path, std::string& out, std::string& error) override;

private:
    QUrl base_;
    std::unique_ptr<QNetworkAccessManager> nam_;
};
