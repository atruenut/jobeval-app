#include "JobResult.hpp"

#include <algorithm>

#include "evalcore/scoring.hpp"

using namespace jobeval;

const QList<Criterion>& criteria() {
    static const QList<Criterion> list = {
        {"money", QObject::tr("Money"), QObject::tr("Net value per month after tax, benefits and costs."), 8, true, QString()},
        {"hourly", QObject::tr("Pay per hour of your life"), QObject::tr("Real hourly: money divided by all the time the job takes."), 6, true, QString()},
        {"time", QObject::tr("Free time"), QObject::tr("Fewer hours spent on the job and getting to it."), 5, true, QString()},
        {"stability", QObject::tr("Job security"), QObject::tr("How safe the job is: permanent, stable employer, steady hours."), 5, false, "s_stability"},
        {"growth", QObject::tr("Career growth"), QObject::tr("Promotion path, raises, what the job does for your future pay."), 4, false, "s_growth"},
        {"interest", QObject::tr("Interesting work"), QObject::tr("How much you would enjoy or find meaning in the work itself."), 5, false, "s_interest"},
        {"flex", QObject::tr("Flexibility"), QObject::tr("Control over your schedule, predictable shifts, time off when you need it."), 4, false, "s_flex"},
        {"culture", QObject::tr("People and culture"), QObject::tr("Boss, coworkers and workplace atmosphere."), 4, false, "s_culture"},
        {"stress", QObject::tr("Low stress"), QObject::tr("10 = relaxed, 0 = physically or mentally draining."), 4, false, "s_stress"},
        {"learning", QObject::tr("Learning"), QObject::tr("Training and skills you pick up."), 3, false, "s_learning"},
        {"location", QObject::tr("Location and setting"), QObject::tr("The area, the building, the surroundings."), 3, false, "s_location"},
    };
    return list;
}

QVariantMap defaultPriorities() {
    QVariantMap m;
    for (const Criterion& c : criteria()) m[c.key] = c.defaultWeight;
    return m;
}

QVariantMap normalizedPriorities(const QVariantMap& saved) {
    QVariantMap m = defaultPriorities();
    for (const Criterion& c : criteria()) {
        auto it = saved.find(c.key);
        if (it == saved.end()) continue;
        bool ok = false;
        const double v = it.value().toDouble(&ok);
        if (ok) m[c.key] = qBound(0.0, v, 10.0);
    }
    return m;
}

QColor seriesColor(int index) {
    static const QList<QColor> palette = {QColor("#1a5fb4"), QColor("#e66100"), QColor("#26a269"), QColor("#c01c8c"),
                                          QColor("#865e3c"), QColor("#0b7285"), QColor("#a51d2d"), QColor("#6f4ab3")};
    return palette[index % palette.size()];
}

JobResult evaluateJob(const Job& job, const BundleLookup& lookup, const QString& regionName) {
    JobResult r;
    r.job = job;
    r.region = regionName;
    r.notWorking = job.state.value("kind").toInt() == 1;
    auto bundle = lookup(job.packId);
    if (!bundle) {
        r.problem = QObject::tr("rules not downloaded");
        return r;
    }
    r.currency = QString::fromStdString(bundle->currency);
    std::vector<TraceEntry> trace;
    std::string err;
    if (r.notWorking) {
        r.ok = evaluate_not_working(bundle, notWorkingFromState(job.state), r.nw, trace, err);
        if (!r.ok) {
            r.problem = QString::fromStdString(err);
            return r;
        }
        r.monthlyValue = r.nw.net_monthly.to_double();
        r.hoursPerWeek = r.nw.activity_hours_year.to_double() / 52.0;
        return r;
    }
    r.ok = evaluate_job(bundle, inputsFromState(job.state), r.emp, trace, err);
    if (!r.ok) {
        r.problem = QString::fromStdString(err);
        return r;
    }
    r.monthlyValue = r.emp.real.value_annual.to_double() / 12.0;
    const double weeks = job.state.value("weeks").toDouble();
    r.hoursPerWeek = weeks > 0 ? r.emp.real.total_hours.to_double() / weeks : 0.0;
    r.hasHourly = !r.emp.real.total_hours.is_zero();
    r.realHourly = r.hasHourly ? r.emp.real.real_hourly.to_double() : 0.0;
    return r;
}

void scoreResults(QList<JobResult>& results, const QVariantMap& priorities) {
    const QVariantMap prio = normalizedPriorities(priorities);
    QMap<QString, double> bestMoney, bestHourly;
    for (const JobResult& r : results) {
        if (!r.ok) continue;
        bestMoney[r.currency] = std::max(bestMoney.value(r.currency, 0.0), r.monthlyValue);
        if (r.hasHourly) bestHourly[r.currency] = std::max(bestHourly.value(r.currency, 0.0), r.realHourly);
    }
    auto D = [](double v) { return Decimal::parse(QString::number(v, 'f', 6).toStdString()); };
    std::map<std::string, Decimal> weights;
    for (const Criterion& c : criteria()) weights[c.key.toStdString()] = D(prio[c.key].toDouble());

    for (int i = 0; i < results.size(); ++i) {
        JobResult& r = results[i];
        r.color = seriesColor(i);
        r.scores.clear();
        r.hasFit = false;
        if (!r.ok) continue;
        std::map<std::string, Decimal> sc;
        auto put = [&](const QString& key, double v) {
            r.scores[key] = v;
            sc[key.toStdString()] = D(v);
        };
        put("money", scale_score(D(r.monthlyValue), Decimal(), D(bestMoney.value(r.currency))).to_double());
        if (r.hasHourly) put("hourly", scale_score(D(r.realHourly), Decimal(), D(bestHourly.value(r.currency))).to_double());
        put("time", scale_score(D(r.hoursPerWeek), Decimal::from_int(60), Decimal()).to_double());
        for (const Criterion& c : criteria())
            if (!c.computed) put(c.key, r.job.state.value(c.stateKey, 5.0).toDouble());
        Decimal fit;
        r.hasFit = weighted_fit(sc, weights, fit);
        r.fit = fit.to_double();
    }
}

QList<int> sortOrder(const QList<JobResult>& results, int mode, const std::function<QString(const QString&)>& regionTitle) {
    const int n = static_cast<int>(results.size());
    auto key = [&](int i, double& v) {
        const JobResult& r = results[i];
        switch (mode) {
            case 4: case 5: if (!r.ok || r.notWorking) return false; v = r.emp.hourly_wage.to_double(); return true;
            case 6: if (!r.ok || !r.hasHourly) return false; v = r.realHourly; return true;
            case 7: if (!r.ok) return false; v = r.notWorking ? r.nw.net_monthly.to_double() : r.emp.take_home.net_monthly.to_double(); return true;
            case 8: if (!r.ok) return false; v = r.hoursPerWeek; return true;
            case 9: if (!r.ok || !r.hasFit) return false; v = r.fit; return true;
            default: return false;
        }
    };
    QList<int> order;
    for (int i = 0; i < n; ++i) order.append(i);
    if (mode == 1) {
        std::reverse(order.begin(), order.end());
    } else if (mode == 2 || mode == 3 || mode == 10) {
        std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
            const int c = mode == 10 ? regionTitle(results[a].job.packId).localeAwareCompare(regionTitle(results[b].job.packId))
                                     : results[a].job.name.localeAwareCompare(results[b].job.name);
            return mode == 3 ? c > 0 : c < 0;
        });
    } else if (mode >= 4) {
        const bool ascending = mode == 5 || mode == 8;
        std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
            double va = 0, vb = 0;
            const bool ha = key(a, va), hb = key(b, vb);
            if (ha != hb) return ha;
            if (!ha) return false;
            return ascending ? va < vb : va > vb;
        });
    }
    return order;
}
