#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>

// Optional, person-triggered lookup of exchange rates (European Central Bank reference
// rates, republished by the open Frankfurter API; no key, no account). GUI-free so it can
// be unit tested. The request carries only currency codes.
namespace fx {

// Query: "1 <home> = x <other>" for each other currency.
QUrl latestUrl(const QString& home, const QStringList& others);

struct Parsed {
    bool ok = false;
    QString error;
    QString date;            // publication date reported by the API
    QVariantMap rates;       // other currency -> units of `home` that one unit is worth
    QStringList missing;     // requested currencies the API did not return
};

// Turns the API reply into the app's convention (1 <other> = rate <home>) by inverting.
Parsed parseLatest(const QByteArray& json, const QString& home, const QStringList& others);

}  // namespace fx
