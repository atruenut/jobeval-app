#include "JobModel.hpp"

#include <QDate>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

using namespace jobeval;

const QList<FieldSpec>& fieldSpecs() {
    static const QList<FieldSpec> specs = {
        // key        default min  max       decimals
        {"basis", 0, 0, 1, 0},        // 0 hourly wage, 1 annual salary
        {"pay", 20, 0, 9999999, 2},   // wage per hour, or salary per year
        {"hours", 40, 0, 168, 1},
        {"weeks", 52, 0, 52, 0},
        {"vacation", 0, 0, 52, 0},    // paid vacation weeks (inside paid weeks)
        {"daysoff", 0, 0, 365, 1},    // other paid days not worked
        {"days", 5, 0, 7, 1},
        {"otmode", 1, 0, 2, 0},       // 0 none, 1 legal minimum, 2 company policy
        {"otmult", 1.5, 0, 10, 2},
        {"otlegal", 1, 0, 1, 0},      // company overtime starts where the law's does
        {"otthr", 40, 0, 168, 1},
        {"bonus", 0, 0, 9999999, 2},
        {"premium", 0, 0, 9999, 2},
        {"premshare", 100, 0, 100, 1},
        {"distance", 0, 0, 999, 1},
        {"minutes", 0, 0, 600, 0},
        {"costper", 0, 0, 99, 3},
        {"parking", 0, 0, 9999, 2},
        {"transit", 0, 0, 9999, 2},
        {"benefits", 0, 0, 999999, 2},
        {"premiums", 0, 0, 999999, 2},
        {"pension", 0, 0, 100, 1},
        {"otherben", 0, 0, 9999999, 2},
        {"workcosts", 0, 0, 9999999, 2},
        {"unpaid", 0, 0, 600, 0},
        {"unit", 0, 0, 1, 0},         // 0 km, 1 mi (display only)
        {"kind", 0, 0, 1, 0},         // 0 employment, 1 not working
        {"remote", 0, 0, 7, 1},       // days per week worked from home
        {"signing", 0, 0, 9999999, 2},
        {"stay", 2, 0.5, 40, 1},      // years you expect to stay (spreads the signing bonus)
        {"raise", 0, 0, 50, 1},       // expected annual raise, percent
        // How the job feels, 0-10 (higher is better; stress is scored as "low stress").
        {"s_stability", 5, 0, 10, 0},
        {"s_growth", 5, 0, 10, 0},
        {"s_interest", 5, 0, 10, 0},
        {"s_flex", 5, 0, 10, 0},
        {"s_culture", 5, 0, 10, 0},
        {"s_stress", 5, 0, 10, 0},
        {"s_learning", 5, 0, 10, 0},
        {"s_location", 5, 0, 10, 0},
        // Not-working situations.
        {"nw_kind", 0, 0, 3, 0},      // 0 unemployment, 1 student, 2 savings / gap, 3 other
        {"nw_ei", 1, 0, 1, 0},        // use the region's unemployment benefit formula
        {"nw_prior", 0, 0, 9999999, 2},
        {"nw_weeks", 0, 0, 520, 0},
        {"nw_taxable", 0, 0, 999999, 2},
        {"nw_taxfree", 0, 0, 999999, 2},
        {"nw_expenses", 0, 0, 999999, 2},
        {"nw_savings", 0, 0, 99999999, 2},
        {"nw_hours", 0, 0, 168, 1},
    };
    return specs;
}

const FieldSpec* fieldSpec(const QString& key) {
    for (const FieldSpec& f : fieldSpecs())
        if (key == f.key) return &f;
    return nullptr;
}

QString newJobId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }

QVariantMap defaultState() {
    QVariantMap m;
    for (const FieldSpec& f : fieldSpecs()) m[f.key] = f.def;
    m["asof"] = QDate::currentDate().toString(Qt::ISODate);
    return m;
}

QVariantMap normalizedState(const QVariantMap& saved) {
    QVariantMap m = defaultState();
    for (const FieldSpec& f : fieldSpecs()) {
        auto it = saved.find(f.key);
        if (it == saved.end()) continue;
        bool ok = false;
        double v = it.value().toDouble(&ok);
        if (ok && v == v) m[f.key] = qBound(f.min, v, f.max);
    }
    const QDate d = QDate::fromString(saved.value("asof").toString(), Qt::ISODate);
    if (d.isValid()) m["asof"] = d.toString(Qt::ISODate);
    for (auto it = saved.begin(); it != saved.end(); ++it) {  // pack-declared extras keep their key
        bool ok = false;
        const double v = it.value().toDouble(&ok);
        if (it.key().startsWith("x.") && ok && v == v) m[it.key()] = v;
    }
    return m;
}

static Decimal dec(const QVariantMap& s, const char* key) {
    const FieldSpec* f = fieldSpec(key);
    const double v = s.value(key, f ? f->def : 0.0).toDouble();
    return Decimal::parse(QString::number(v, 'f', f ? f->decimals : 2).toStdString());
}

JobInputs inputsFromState(const QVariantMap& state) {
    const QVariantMap s = normalizedState(state);
    JobInputs j;
    j.basis = s["basis"].toInt() == 1 ? PayBasis::Salary : PayBasis::Hourly;
    j.pay = dec(s, "pay");
    j.hours_per_week = dec(s, "hours");
    j.paid_weeks = dec(s, "weeks");
    const int om = s["otmode"].toInt();
    j.overtime = om == 0 ? OvertimeMode::None : om == 2 ? OvertimeMode::Company : OvertimeMode::Legal;
    j.company_multiplier = dec(s, "otmult");
    j.use_legal_threshold = s["otlegal"].toInt() != 0;
    j.company_threshold_hours = dec(s, "otthr");
    j.bonus_annual = dec(s, "bonus");
    j.signing_bonus = dec(s, "signing");
    j.expected_years = dec(s, "stay");
    j.remote_days_per_week = dec(s, "remote");
    j.shift_premium_per_hour = dec(s, "premium");
    j.shift_premium_percent = dec(s, "premshare");
    j.paid_vacation_weeks = dec(s, "vacation");
    j.paid_days_off = dec(s, "daysoff");
    j.days_per_week = dec(s, "days");
    j.unpaid_minutes_per_day = dec(s, "unpaid");
    j.commute_distance = dec(s, "distance");
    j.commute_minutes = dec(s, "minutes");
    j.cost_per_distance = dec(s, "costper");
    j.daily_parking_and_tolls = dec(s, "parking");
    j.daily_transit_fare = dec(s, "transit");
    j.work_costs_annual = dec(s, "workcosts");
    j.benefits_monthly_value = dec(s, "benefits");
    j.benefit_premiums_monthly = dec(s, "premiums");
    j.pension_match_percent = dec(s, "pension");
    j.other_benefits_annual = dec(s, "otherben");
    j.as_of = s["asof"].toString().toStdString();
    for (auto it = s.begin(); it != s.end(); ++it) {
        if (!it.key().startsWith("x.")) continue;
        j.extra[it.key().mid(2).toStdString()] = Value::num(Decimal::parse(QString::number(it.value().toDouble(), 'f', 2).toStdString()));
    }
    return j;
}

NotWorkingInputs notWorkingFromState(const QVariantMap& state) {
    const QVariantMap s = normalizedState(state);
    NotWorkingInputs n;
    n.use_unemployment_benefit = s["nw_ei"].toInt() != 0 && s["nw_kind"].toInt() == 0;
    n.prior_earnings_annual = dec(s, "nw_prior");
    n.benefit_weeks = dec(s, "nw_weeks");
    n.other_taxable_monthly = dec(s, "nw_taxable");
    n.tax_free_monthly = dec(s, "nw_taxfree");
    n.monthly_expenses = dec(s, "nw_expenses");
    n.savings = dec(s, "nw_savings");
    n.activity_hours_per_week = dec(s, "nw_hours");
    n.as_of = s["asof"].toString().toStdString();
    for (auto it = s.begin(); it != s.end(); ++it) {
        if (!it.key().startsWith("x.")) continue;
        n.extra[it.key().mid(2).toStdString()] = Value::num(Decimal::parse(QString::number(it.value().toDouble(), 'f', 2).toStdString()));
    }
    return n;
}

QByteArray projectToJson(const Project& p, const QString& kind) {
    QJsonArray jobs;
    for (const Job& j : p.jobs) {
        QJsonObject o;
        o["id"] = j.id;
        o["name"] = j.name;
        o["notes"] = j.notes;
        o["region"] = j.packId;
        QJsonObject st;
        for (auto it = j.state.begin(); it != j.state.end(); ++it) {
            if (it.key() == "asof") st[it.key()] = it.value().toString();
            else st[it.key()] = it.value().toDouble();
        }
        o["inputs"] = st;
        jobs.append(o);
    }
    QJsonObject root;
    root["format"] = "jobeval";
    root["version"] = kFileVersion;
    root["kind"] = kind;
    root["name"] = p.name;
    root["jobs"] = jobs;
    QJsonObject pr;
    for (auto it = p.priorities.begin(); it != p.priorities.end(); ++it) pr[it.key()] = it.value().toDouble();
    if (!pr.isEmpty()) root["priorities"] = pr;
    if (!p.baselineId.isEmpty()) root["baseline"] = p.baselineId;
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool projectFromJson(const QByteArray& json, Project& out, QString& error) {
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (doc.isNull() || !doc.isObject()) {
        error = QObject::tr("This is not a valid Job Evaluator file (%1).").arg(pe.errorString());
        return false;
    }
    const QJsonObject root = doc.object();
    if (root["format"].toString() != "jobeval") {
        error = QObject::tr("This file was not made by Job Evaluator.");
        return false;
    }
    const int version = root["version"].toInt(0);
    if (version < 1 || version > kFileVersion) {
        error = version > kFileVersion ? QObject::tr("This file was saved by a newer version of Job Evaluator (file version %1). Update the app to open it.").arg(version)
                                       : QObject::tr("Unrecognised file version.");
        return false;
    }
    Project p;
    p.name = root["name"].toString();
    for (const QJsonValue& v : root["jobs"].toArray()) {
        const QJsonObject o = v.toObject();
        Job j;
        j.id = o["id"].toString();
        if (j.id.isEmpty()) j.id = newJobId();
        j.name = o["name"].toString();
        if (j.name.isEmpty()) j.name = QObject::tr("Untitled job");
        j.notes = o["notes"].toString();
        j.packId = o["region"].toString();
        QVariantMap raw;
        const QJsonObject st = o["inputs"].toObject();
        for (auto it = st.begin(); it != st.end(); ++it) raw[it.key()] = it.value().isString() ? QVariant(it.value().toString()) : QVariant(it.value().toDouble());
        j.state = normalizedState(raw);
        p.jobs.append(j);
    }
    const QJsonObject pr = root["priorities"].toObject();
    for (auto it = pr.begin(); it != pr.end(); ++it) {
        const double v = it.value().toDouble(-1);
        if (v >= 0 && v <= 10) p.priorities[it.key()] = v;
    }
    p.baselineId = root["baseline"].toString();
    out = p;
    return true;
}

bool writeProjectFile(const QString& path, const Project& p, const QString& kind, QString& error) {
    QSaveFile f(path);  // writes to a temporary file and renames, so a crash never leaves a half file
    if (!f.open(QIODevice::WriteOnly)) {
        error = QObject::tr("Cannot write %1: %2").arg(path, f.errorString());
        return false;
    }
    f.write(projectToJson(p, kind));
    if (!f.commit()) {
        error = QObject::tr("Cannot save %1: %2").arg(path, f.errorString());
        return false;
    }
    return true;
}

bool readProjectFile(const QString& path, Project& out, QString& error) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        error = QObject::tr("Cannot open %1: %2").arg(path, f.errorString());
        return false;
    }
    return projectFromJson(f.readAll(), out, error);
}
