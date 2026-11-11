#pragma once

#include <QWidget>
#include <functional>
#include <memory>

#include "JobModel.hpp"
#include "evalcore/pack.hpp"

class QLabel;
class QTableWidget;

// Jobs side by side. Ranking is by real hourly pay, and only between jobs paid in the same
// currency (a Canadian and a US job are shown but never ranked against each other).
class ComparePage : public QWidget {
    Q_OBJECT
public:
    using BundleLookup = std::function<std::shared_ptr<const jobeval::Bundle>(const QString& packId)>;
    explicit ComparePage(QWidget* parent = nullptr);
    void refresh(const QList<Job>& jobs, const BundleLookup& lookup, const QMap<QString, QString>& regionNames);

private:
    QLabel* summary_;
    QLabel* warning_;
    QLabel* empty_;
    QTableWidget* table_;
};
