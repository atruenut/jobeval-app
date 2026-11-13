#include "FxRates.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

namespace fx {

QUrl latestUrl(const QString& home, const QStringList& others) {
    QUrl u("https://api.frankfurter.dev/v1/latest");
    QUrlQuery q;
    q.addQueryItem("base", home);
    q.addQueryItem("symbols", others.join(','));
    u.setQuery(q);
    return u;
}

Parsed parseLatest(const QByteArray& json, const QString& home, const QStringList& others) {
    Parsed p;
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        p.error = QStringLiteral("unreadable reply");
        return p;
    }
    const QJsonObject o = doc.object();
    if (o.contains("base") && o["base"].toString() != home) {
        p.error = QStringLiteral("reply is for a different currency");
        return p;
    }
    p.date = o["date"].toString();
    const QJsonObject rates = o["rates"].toObject();
    for (const QString& cur : others) {
        const double perHome = rates.value(cur).toDouble();   // units of `cur` per 1 `home`
        if (perHome > 1e-9 && perHome < 1e9) p.rates[cur] = 1.0 / perHome;
        else p.missing << cur;
    }
    p.ok = !p.rates.isEmpty();
    if (!p.ok) p.error = QStringLiteral("no rates for these currencies");
    return p;
}

}  // namespace fx
