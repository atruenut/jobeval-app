#pragma once

#include <QLocale>
#include <QString>

#include "evalcore/decimal.hpp"

// Display only: formats a Decimal for the screen with grouping and the currency code.
inline QString formatMoney(const jobeval::Decimal& d, const std::string& currency) {
    QString s = QLocale().toString(d.to_double(), 'f', 2);
    if (!currency.empty()) s += " " + QString::fromStdString(currency);
    return s;
}

inline QString formatNumber(const jobeval::Decimal& d, int places = 2) {
    return QLocale().toString(d.to_double(), 'f', places);
}
