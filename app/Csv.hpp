#pragma once

#include <QString>
#include <QStringList>

// Minimal CSV writer (RFC 4180 quoting). Text starting with = + - or @ that is not a number
// gets a leading single quote so spreadsheets do not run it as a formula.
namespace csv {

inline QString field(const QString& s) {
    QString v = s;
    bool num = false;
    v.toDouble(&num);
    if (!num && !v.isEmpty() && QStringLiteral("=+-@").contains(v[0])) v.prepend('\'');
    if (v.contains(',') || v.contains('"') || v.contains('\n') || v.contains('\r') || v != s) {
        v.replace('"', "\"\"");
        return '"' + v + '"';
    }
    return v;
}

inline QString row(const QStringList& cells) {
    QStringList out;
    for (const QString& c : cells) out << field(c);
    return out.join(',') + "\r\n";
}

}  // namespace csv
