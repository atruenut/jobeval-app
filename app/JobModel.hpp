#pragma once

#include <QList>
#include <QString>
#include <QVariantMap>
#include <memory>

#include "evalcore/jobcalc.hpp"
#include "evalcore/notworking.hpp"

// A job as the user entered it. Everything numeric lives in `state` under the keys listed in
// fieldSpecs(); the engine inputs are derived from it, never stored.
struct Job {
    QString id;
    QString name;
    QString notes;
    QString packId;  // region rules this job is evaluated under
    QVariantMap state;
};

struct Project {
    QString name;
    QList<Job> jobs;
    QVariantMap priorities;  // criterion key -> importance 0..10 (see criteria() in JobResult.hpp)
    QString baselineId;      // a not-working job that Compare measures jobs against ("" = none)
    QString homeCurrency;    // Compare converts other currencies into this one ("" = automatic)
    QString ratesInfo;       // where/when the rates came from, e.g. "ECB via Frankfurter, 2026-11-12" ("" = typed by hand)
    QVariantMap rates;       // currency code -> how many units of homeCurrency one unit is worth (typed by the person)
};

struct FieldSpec {
    const char* key;
    double def;
    double min;
    double max;
    int decimals;
};
const QList<FieldSpec>& fieldSpecs();
const FieldSpec* fieldSpec(const QString& key);

QString newJobId();
QVariantMap defaultState();           // all keys at their defaults, "asof" = today
QVariantMap normalizedState(const QVariantMap& saved);  // defaults for anything missing, clamped to range

// Engine inputs for a job; `bundle` supplies the pack's own input names for "x.<name>" extras.
jobeval::JobInputs inputsFromState(const QVariantMap& state);
jobeval::NotWorkingInputs notWorkingFromState(const QVariantMap& state);

// File format: JSON, {"format":"jobeval","version":1,"kind":"project"|"jobs","name":...,"jobs":[...]}.
// Reading ignores unknown fields; a newer version is refused with a clear message.
constexpr int kFileVersion = 1;
bool writeProjectFile(const QString& path, const Project& p, const QString& kind, QString& error);
bool readProjectFile(const QString& path, Project& out, QString& error);
QByteArray projectToJson(const Project& p, const QString& kind);
bool projectFromJson(const QByteArray& json, Project& out, QString& error);
