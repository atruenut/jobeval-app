#include "NetworkFetcher.hpp"

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

NetworkFetcher::NetworkFetcher(QUrl base) : base_(std::move(base)) {
    QString p = base_.path();
    if (!p.endsWith('/')) base_.setPath(p + "/");
}

NetworkFetcher::~NetworkFetcher() = default;

bool NetworkFetcher::get(const std::string& path, std::string& out, std::string& error) {
    if (!nam_) nam_ = std::make_unique<QNetworkAccessManager>();  // created on the worker thread that uses it
    QNetworkRequest req(base_.resolved(QUrl(QString::fromStdString(path))));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(20000);
    req.setHeader(QNetworkRequest::UserAgentHeader, "jobeval/" JOBEVAL_VERSION);
    std::unique_ptr<QNetworkReply> reply(nam_->get(req));
    QEventLoop loop;
    QObject::connect(reply.get(), &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    if (reply->error() != QNetworkReply::NoError) {
        error = reply->errorString().toStdString();
        return false;
    }
    const QByteArray data = reply->readAll();
    if (data.size() > 16 * 1024 * 1024) {
        error = "response is too large";
        return false;
    }
    out.assign(data.constData(), static_cast<size_t>(data.size()));
    return true;
}
