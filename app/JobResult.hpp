#pragma once

#include <QColor>
#include <QList>
#include <QMap>
#include <QString>
#include <functional>
#include <memory>

#include "JobModel.hpp"

// One evaluated job or not-working situation, with the derived figures the comparison views use.
struct JobResult {
    Job job;
    bool notWorking = false;
    bool ok = false;
    QString problem;            // why !ok
    QString currency;
    QString region;             // display name
    jobeval::JobOutcome emp;    // valid when !notWorking && ok
    jobeval::NotWorkingOutcome nw;  // valid when notWorking && ok
    double monthlyValue = 0;    // net value per month (jobs) or net income per month (not working)
    double hoursPerWeek = 0;    // time the job takes, or time the situation uses
    double realHourly = 0;      // jobs only (0 for not working)
    bool hasHourly = false;
    QMap<QString, double> scores;  // criterion key -> 0..10 (filled by scoreResults)
    double fit = 0;                // 0..100 (filled by scoreResults)
    bool hasFit = false;
    QColor color;
};

// A decision criterion. "computed" ones come from the numbers, the rest from the job's 0-10 ratings.
struct Criterion {
    QString key;
    QString label;
    QString help;
    double defaultWeight;
    bool computed;
    QString stateKey;  // rating field for subjective criteria
};
const QList<Criterion>& criteria();
QVariantMap defaultPriorities();
QVariantMap normalizedPriorities(const QVariantMap& saved);

using BundleLookup = std::function<std::shared_ptr<const jobeval::Bundle>(const QString& packId)>;

JobResult evaluateJob(const Job& job, const BundleLookup& lookup, const QString& regionName);

// Fills in per-criterion scores and the weighted fit. Money and hourly are scored relative to the best
// in the same currency (the best earns 10), free time on an absolute scale (0 h/week = 10, 60 h = 0).
void scoreResults(QList<JobResult>& results, const QVariantMap& priorities);

QColor seriesColor(int index);
