#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// Compares dotted version strings such as "0.7.1" or "v0.10.0". Returns -1, 0 or 1.
inline int compareVersions(const QString& a, const QString& b) {
    auto parts = [](QString s) {
        if (s.startsWith('v') || s.startsWith('V')) s.remove(0, 1);
        QList<int> out;
        for (const QString& p : s.split('.')) out << p.toInt();
        return out;
    };
    QList<int> x = parts(a), y = parts(b);
    const int n = qMax(x.size(), y.size());
    for (int i = 0; i < n; ++i) {
        const int xi = i < x.size() ? x[i] : 0, yi = i < y.size() ? y[i] : 0;
        if (xi != yi) return xi < yi ? -1 : 1;
    }
    return 0;
}
