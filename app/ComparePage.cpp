#include "ComparePage.hpp"

#include <QAction>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>

#include "Format.hpp"
#include "evalcore/jobcalc.hpp"

using namespace jobeval;

namespace {

QString money(double v, const QString& cur) { return QLocale().toString(v, 'f', 2) + " " + cur; }

Decimal D(double v) { return Decimal::parse(QString::number(v, 'f', 6).toStdString()); }

struct Row {
    QString key, label;
    int better;  // +1 higher is better, -1 lower, 0 no highlight
};

const QList<Row>& rows() {
    static const QList<Row> r = {
        {"region", ComparePage::tr("Region"), 0},
        {"kind", ComparePage::tr("Type"), 0},
        {"fit", ComparePage::tr("Fit score (0-100, from your priorities)"), +1},
        {"hourly", ComparePage::tr("Real hourly"), +1},
        {"paid", ComparePage::tr("Per paid hour, after tax"), +1},
        {"vsbase", ComparePage::tr("Real hourly extra over the baseline"), +1},
        {"vsbasemonth", ComparePage::tr("Extra per month over the baseline"), +1},
        {"month", ComparePage::tr("Take-home per month"), +1},
        {"gross", ComparePage::tr("Gross pay per year"), +1},
        {"benefits", ComparePage::tr("Employer benefits per year"), +1},
        {"costs", ComparePage::tr("Commute, work costs and premiums per year"), -1},
        {"value", ComparePage::tr("Net value per year"), +1},
        {"hours", ComparePage::tr("Hours per week it takes"), -1},
    };
    return r;
}

}  // namespace

ComparePage::ComparePage(QWidget* parent) : QWidget(parent) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(20, 16, 20, 16);
    auto* title = new QLabel(tr("Compare"));
    title->setObjectName("pageTitle");
    v->addWidget(title);
    tabs_ = new QTabWidget;
    v->addWidget(tabs_, 1);

    // ---- Overview
    auto* ov = new QWidget;
    auto* ovl = new QVBoxLayout(ov);
    summary_ = new QLabel;
    summary_->setWordWrap(true);
    summary_->setTextFormat(Qt::RichText);
    ovl->addWidget(summary_);
    warning_ = new QLabel;
    warning_->setWordWrap(true);
    warning_->setObjectName("muted");
    ovl->addWidget(warning_);
    auto* bar = new QHBoxLayout;
    bar->addWidget(new QLabel(tr("Baseline (what you would do instead):")));
    baseline_ = new QComboBox;
    baseline_->setToolTip(tr("Pick a not-working situation (unemployment, studying, living on savings). Each job is then also shown as the extra it gives you over it."));
    baseline_->setMinimumWidth(220);
    bar->addWidget(baseline_);
    bar->addStretch(1);
    rowsButton_ = new QToolButton;
    rowsButton_->setText(tr("Rows shown…"));
    rowsButton_->setPopupMode(QToolButton::InstantPopup);
    auto* menu = new QMenu(rowsButton_);
    for (const Row& r : rows()) {
        rowVisible_[r.key] = true;
        QAction* a = menu->addAction(r.label);
        a->setCheckable(true);
        a->setChecked(true);
        const QString key = r.key;
        connect(a, &QAction::toggled, this, [this, key](bool on) {
            rowVisible_[key] = on;
            if (!updating_) buildOverview();
        });
    }
    rowsButton_->setMenu(menu);
    bar->addWidget(rowsButton_);
    ovl->addLayout(bar);
    empty_ = new QLabel(tr("Add a job to start. Add two or more to compare them here."));
    empty_->setObjectName("muted");
    ovl->addWidget(empty_);
    table_ = new QTableWidget;
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setShowGrid(false);
    table_->setAlternatingRowColors(true);
    table_->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->horizontalHeader()->setMinimumSectionSize(140);
    ovl->addWidget(table_, 1);
    tabs_->addTab(ov, tr("Overview"));
    connect(baseline_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (updating_) return;
        baselineId_ = baseline_->currentData().toString();
        Q_EMIT baselineChanged(baselineId_);
        rebuild();
    });

    // ---- Charts
    auto* ch = new QWidget;
    auto* chl = new QVBoxLayout(ch);
    auto* chBar = new QHBoxLayout;
    chBar->addWidget(new QLabel(tr("Show:")));
    chartKind_ = new QComboBox;
    chartKind_->addItem(tr("Real hourly pay"), ChartWidget::HourlyBars);
    chartKind_->addItem(tr("Where the money goes (per year)"), ChartWidget::MoneyFlow);
    chartKind_->addItem(tr("Time versus money"), ChartWidget::TimeMoney);
    chartKind_->addItem(tr("Where the time goes (per week)"), ChartWidget::TimeSplit);
    chartKind_->addItem(tr("Scorecard (radar)"), ChartWidget::Radar);
    chBar->addWidget(chartKind_);
    chBar->addStretch(1);
    chl->addLayout(chBar);
    chartNote_ = new QLabel;
    chartNote_->setObjectName("muted");
    chartNote_->setWordWrap(true);
    chl->addWidget(chartNote_);
    chart_ = new ChartWidget;
    chl->addWidget(chart_, 1);
    tabs_->addTab(ch, tr("Charts"));
    connect(chartKind_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        chart_->setMode(static_cast<ChartWidget::Mode>(chartKind_->currentData().toInt()));
        static const QStringList notes = {
            tr("Per paid hour is take-home divided by the hours you are paid for. Real hourly also counts commuting, work costs, unpaid time and the value of benefits."),
            tr("Each bar is what the job pays in a year (plus benefits), split into tax, contributions, costs of having the job, and what is left."),
            tr("Up and to the left is best: more money for less of your time. Not-working situations are included."),
            tr("Hours in a typical week: working, commuting and unpaid time at work. Not-working situations show your commitments."),
            tr("Every criterion scored 0-10 (10 on the outside). Subjective ones come from your sliders on each job's page."),
        };
        chartNote_->setText(notes.value(chartKind_->currentIndex()));
    });
    chartNote_->setText(tr("Per paid hour is take-home divided by the hours you are paid for. Real hourly also counts commuting, work costs, unpaid time and the value of benefits."));

    // ---- Fit
    auto* fit = new QWidget;
    auto* fl = new QHBoxLayout(fit);
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFixedWidth(330);
    auto* sw = new QWidget;
    auto* sg = new QGridLayout(sw);
    auto* hint = new QLabel(tr("How much does each thing matter to you? 0 ignores it, 10 is crucial. Jobs are scored 0-10 on each and combined by these weights."));
    hint->setWordWrap(true);
    hint->setObjectName("muted");
    sg->addWidget(hint, 0, 0, 1, 3);
    int row = 1;
    for (const Criterion& c : criteria()) {
        auto* lab = new QLabel(c.label);
        lab->setToolTip(c.help);
        auto* sl = new QSlider(Qt::Horizontal);
        sl->setRange(0, 10);
        sl->setToolTip(c.help);
        auto* val = new QLabel("0");
        val->setMinimumWidth(18);
        sg->addWidget(lab, row, 0);
        sg->addWidget(sl, row, 1);
        sg->addWidget(val, row, 2);
        sliders_[c.key] = sl;
        sliderValues_[c.key] = val;
        connect(sl, &QSlider::valueChanged, this, [this](int) { onPriorityEdited(); });
        ++row;
    }
    sg->setRowStretch(row, 1);
    scroll->setWidget(sw);
    fl->addWidget(scroll);
    auto* right = new QVBoxLayout;
    fitText_ = new QLabel;
    fitText_->setWordWrap(true);
    fitText_->setTextFormat(Qt::RichText);
    right->addWidget(fitText_);
    fitChart_ = new ChartWidget;
    fitChart_->setMode(ChartWidget::FitRanking);
    right->addWidget(fitChart_, 1);
    fl->addLayout(right, 1);
    tabs_->addTab(fit, tr("Fit score"));

    // ---- What-if
    auto* wi = new QWidget;
    auto* wl = new QVBoxLayout(wi);
    auto* form = new QFormLayout;
    adjustJob_ = new QComboBox;
    matchJob_ = new QComboBox;
    form->addRow(tr("What pay would make this job…"), adjustJob_);
    form->addRow(tr("…as good per hour of your time as:"), matchJob_);
    wl->addLayout(form);
    breakEven_ = new QLabel;
    breakEven_->setWordWrap(true);
    breakEven_->setTextFormat(Qt::RichText);
    wl->addWidget(breakEven_);
    auto* yr = new QHBoxLayout;
    yr->addWidget(new QLabel(tr("Looking ahead: after")));
    years_ = new QSpinBox;
    years_->setRange(0, 30);
    years_->setValue(3);
    years_->setSuffix(tr(" years"));
    yr->addWidget(years_);
    yr->addWidget(new QLabel(tr("of the yearly raise set on the job's page")));
    yr->addStretch(1);
    wl->addLayout(yr);
    futureText_ = new QLabel;
    futureText_->setWordWrap(true);
    futureText_->setTextFormat(Qt::RichText);
    wl->addWidget(futureText_);
    auto* curveNote = new QLabel(tr("Pay curves: how each job's real hourly changes if its pay were lower or higher (50% to 200% of what you entered). Taxes rise faster than pay, so the curve flattens. Dots mark today's pay."));
    curveNote->setWordWrap(true);
    curveNote->setObjectName("muted");
    wl->addWidget(curveNote);
    curveChart_ = new ChartWidget;
    curveChart_->setMode(ChartWidget::PayCurve);
    wl->addWidget(curveChart_, 1);
    tabs_->addTab(wi, tr("What if"));
    auto whatIf = [this]() {
        if (!updating_) buildWhatIf();
    };
    connect(adjustJob_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, whatIf);
    connect(matchJob_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, whatIf);
    connect(years_, QOverload<int>::of(&QSpinBox::valueChanged), this, whatIf);
}

void ComparePage::refresh(const QList<Job>& jobs, const BundleLookup& lookup, const QMap<QString, QString>& regionNames,
                          const QVariantMap& priorities, const QString& baselineId) {
    jobs_ = jobs;
    lookup_ = lookup;
    priorities_ = normalizedPriorities(priorities);
    baselineId_ = baselineId;
    results_.clear();
    for (const Job& j : jobs_) results_.append(evaluateJob(j, lookup_, regionNames.value(j.packId, j.packId)));

    updating_ = true;
    for (const Criterion& c : criteria()) {
        sliders_[c.key]->setValue(qRound(priorities_[c.key].toDouble()));
        sliderValues_[c.key]->setText(QString::number(sliders_[c.key]->value()));
    }
    // Pickers keep their selection by job id.
    auto refill = [&](QComboBox* box, bool notWorkingOnly, bool allowNone) {
        const QString keep = box->currentData().toString();
        box->clear();
        if (allowNone) box->addItem(tr("None (compare jobs only)"), QString());
        for (const JobResult& r : results_) {
            if (notWorkingOnly != r.notWorking && notWorkingOnly) continue;
            if (!notWorkingOnly && (r.notWorking || !r.ok)) continue;
            box->addItem(r.job.name, r.job.id);
        }
        int i = box->findData(keep);
        box->setCurrentIndex(i >= 0 ? i : 0);
    };
    refill(baseline_, true, true);
    {
        const int i = baseline_->findData(baselineId_);
        baseline_->setCurrentIndex(i >= 0 ? i : 0);
    }
    refill(adjustJob_, false, false);
    refill(matchJob_, false, false);
    if (matchJob_->count() > 1 && matchJob_->currentData() == adjustJob_->currentData()) matchJob_->setCurrentIndex(matchJob_->currentIndex() == 0 ? 1 : 0);
    updating_ = false;
    rebuild();
}

void ComparePage::onPriorityEdited() {
    if (updating_) return;
    for (const Criterion& c : criteria()) {
        priorities_[c.key] = sliders_[c.key]->value();
        sliderValues_[c.key]->setText(QString::number(sliders_[c.key]->value()));
    }
    Q_EMIT prioritiesChanged(priorities_);
    rebuild();
}

void ComparePage::rebuild() {
    scoreResults(results_, priorities_);
    buildOverview();
    chart_->setResults(results_, priorities_);
    fitChart_->setResults(results_, priorities_);

    // Fit ranking text, per currency groups are not needed: fit is relative within a currency already.
    QList<const JobResult*> ranked;
    for (const JobResult& r : results_)
        if (r.ok && r.hasFit) ranked.append(&r);
    std::sort(ranked.begin(), ranked.end(), [](const JobResult* a, const JobResult* b) { return a->fit > b->fit; });
    if (ranked.isEmpty()) {
        fitText_->setText(tr("Add jobs and give at least one criterion a weight to see which fits you best."));
    } else {
        QString t = tr("<b>%1</b> fits your priorities best (%2 out of 100).").arg(ranked[0]->job.name.toHtmlEscaped()).arg(ranked[0]->fit, 0, 'f', 0);
        if (ranked.size() > 1) {
            // What pulls the winner ahead of the runner-up: the criterion with the biggest weighted gap.
            const JobResult& a = *ranked[0];
            const JobResult& b = *ranked[1];
            double bestGap = 0;
            QString why;
            for (const Criterion& c : criteria()) {
                if (!a.scores.contains(c.key) || !b.scores.contains(c.key)) continue;
                const double gap = (a.scores[c.key] - b.scores[c.key]) * priorities_[c.key].toDouble();
                if (gap > bestGap) {
                    bestGap = gap;
                    why = c.label;
                }
            }
            if (!why.isEmpty()) t += " " + tr("Its biggest edge over %1 is <b>%2</b>.").arg(b.job.name.toHtmlEscaped(), why.toHtmlEscaped());
        }
        fitText_->setText(t);
    }
    if (!updating_) buildWhatIf();
}

void ComparePage::buildOverview() {
    empty_->setVisible(jobs_.isEmpty());
    table_->setVisible(!jobs_.isEmpty());
    const JobResult* base = nullptr;
    for (const JobResult& r : results_)
        if (r.job.id == baselineId_ && r.notWorking && r.ok) base = &r;

    // Extra over baseline (same currency only).
    auto vsBaseMonth = [&](const JobResult& r, double& out) {
        if (!base || r.notWorking || !r.ok || r.currency != base->currency) return false;
        out = r.monthlyValue - base->monthlyValue;
        return true;
    };
    auto vsBaseHourly = [&](const JobResult& r, double& out) {
        double m;
        if (!vsBaseMonth(r, m) || !r.hasHourly) return false;
        const double totalHours = r.emp.real.total_hours.to_double();
        if (totalHours <= 0) return false;
        out = m * 12.0 / totalHours;
        return true;
    };

    struct Cell {
        QString text;
        bool has = false;
        double v = 0;
    };
    auto cell = [&](const JobResult& r, const QString& key) -> Cell {
        Cell c;
        auto num = [&](double v, bool asMoney = true) {
            c.has = true;
            c.v = v;
            c.text = asMoney ? money(v, r.currency) : QString::number(v, 'f', 1);
        };
        if (key == "region") c.text = r.region;
        else if (key == "kind") c.text = r.notWorking ? tr("Not working") : tr("Job");
        else if (!r.ok) c.text = key == "hourly" ? r.problem : QStringLiteral("–");
        else if (key == "fit") {
            if (r.hasFit) num(r.fit, false), c.text = QString::number(r.fit, 'f', 0);
            else c.text = QStringLiteral("–");
        } else if (key == "hourly") {
            if (r.hasHourly) num(r.realHourly);
            else c.text = QStringLiteral("–");
        } else if (key == "paid") {
            if (!r.notWorking && !r.emp.real.hours_paid.is_zero()) num(r.emp.real.take_home_per_hour_paid.to_double());
            else c.text = QStringLiteral("–");
        } else if (key == "vsbase") {
            double x;
            if (vsBaseHourly(r, x)) num(x);
            else c.text = QStringLiteral("–");
        } else if (key == "vsbasemonth") {
            double x;
            if (vsBaseMonth(r, x)) num(x);
            else c.text = QStringLiteral("–");
        } else if (key == "month") num(r.monthlyValue * 0 + (r.notWorking ? r.nw.net_monthly.to_double() : r.emp.take_home.net_monthly.to_double()));
        else if (key == "gross") {
            if (r.notWorking) c.text = QStringLiteral("–");
            else num(r.emp.take_home.gross_annual.to_double());
        } else if (key == "benefits") {
            if (r.notWorking) c.text = QStringLiteral("–");
            else num(r.emp.real.benefits_value.to_double());
        } else if (key == "costs") {
            if (r.notWorking) c.text = QStringLiteral("–");
            else num((r.emp.real.commute_cost + r.emp.real.work_costs + r.emp.real.benefit_premiums).to_double());
        } else if (key == "value") num(r.monthlyValue * 12.0);
        else if (key == "hours") num(r.hoursPerWeek, false);
        return c;
    };

    QList<Row> shown;
    for (const Row& r : rows()) {
        if (!rowVisible_.value(r.key, true)) continue;
        if ((r.key == "vsbase" || r.key == "vsbasemonth") && !base) continue;
        shown.append(r);
    }
    table_->clear();
    table_->setRowCount(shown.size());
    table_->setColumnCount(results_.size());
    QStringList rowNames;
    for (const Row& r : shown) rowNames << r.label;
    table_->setVerticalHeaderLabels(rowNames);
    QStringList heads;
    for (const JobResult& r : results_) heads << r.job.name;
    table_->setHorizontalHeaderLabels(heads);

    for (int ri = 0; ri < shown.size(); ++ri) {
        const Row& row = shown[ri];
        // Best per currency among comparable cells.
        QMap<QString, double> best;
        QMap<QString, int> count;
        QList<Cell> cells;
        for (const JobResult& r : results_) {
            Cell c = cell(r, row.key);
            cells.append(c);
            if (c.has && row.better != 0 && r.ok) {
                const QString k = row.key == "fit" ? QStringLiteral("*") : r.currency;
                count[k]++;
                if (!best.contains(k) || (row.better > 0 ? c.v > best[k] : c.v < best[k])) best[k] = c.v;
            }
        }
        for (int ci = 0; ci < results_.size(); ++ci) {
            const JobResult& r = results_[ci];
            auto* it = new QTableWidgetItem(cells[ci].text);
            it->setTextAlignment(Qt::AlignCenter);
            const QString k = row.key == "fit" ? QStringLiteral("*") : r.currency;
            if (cells[ci].has && row.better != 0 && count.value(k) > 1 && qFuzzyCompare(cells[ci].v + 1, best[k] + 1)) {
                it->setBackground(QColor(0xd9, 0xf2, 0xdc));
                it->setForeground(QColor(0x14, 0x3d, 0x1c));
                QFont f = it->font();
                f.setBold(true);
                it->setFont(f);
            }
            table_->setItem(ri, ci, it);
        }
    }

    // Summary sentences.
    QMap<QString, QList<const JobResult*>> byCurrency;
    for (const JobResult& r : results_)
        if (r.ok && r.hasHourly) byCurrency[r.currency].append(&r);
    QString sum;
    for (auto it = byCurrency.begin(); it != byCurrency.end(); ++it) {
        auto l = it.value();
        if (l.size() < 2) continue;
        std::sort(l.begin(), l.end(), [](const JobResult* a, const JobResult* b) { return a->realHourly > b->realHourly; });
        const double gap = l[0]->realHourly - l[1]->realHourly;
        sum += (sum.isEmpty() ? "" : "<br>") +
               tr("<b>%1</b> pays the most per hour of your time: %2, which is %3 (%4%) more than %5.")
                   .arg(l[0]->job.name.toHtmlEscaped(), money(l[0]->realHourly, it.key()), money(gap, it.key()),
                        QString::number(l[1]->realHourly > 0 ? gap / l[1]->realHourly * 100.0 : 0.0, 'f', 0), l[1]->job.name.toHtmlEscaped());
    }
    if (base) {
        for (const JobResult& r : results_) {
            double m, h;
            if (vsBaseMonth(r, m) && vsBaseHourly(r, h))
                sum += (sum.isEmpty() ? "" : "<br>") +
                       tr("Compared with <i>%1</i>, <b>%2</b> leaves you %3 %4 per month, or %5 for each hour it takes.")
                           .arg(base->job.name.toHtmlEscaped(), r.job.name.toHtmlEscaped(), m >= 0 ? tr("better off by") : tr("worse off by"),
                                money(std::abs(m), r.currency), money(h, r.currency));
        }
    }
    summary_->setText(sum.isEmpty() && !jobs_.isEmpty() ? tr("Add another job in the same currency to rank them.") : sum);
    warning_->setText(byCurrency.size() > 1 ? tr("Jobs paid in different currencies are shown side by side but never ranked or highlighted against each other.") : QString());
}

void ComparePage::buildWhatIf() {
    // Pay curves for every working job.
    QList<ChartWidget::Curve> curves;
    QString curve_cur;
    for (const JobResult& r : results_) {
        if (!r.ok || r.notWorking) continue;
        auto bundle = lookup_(r.job.packId);
        if (!bundle) continue;
        ChartWidget::Curve c;
        c.name = r.job.name;
        c.color = r.color;
        JobInputs in = inputsFromState(r.job.state);
        const Decimal basePay = in.pay;
        for (int k = 4; k <= 16; ++k) {
            in.pay = basePay * D(k / 8.0);
            JobOutcome o;
            std::vector<TraceEntry> tr_;
            std::string err;
            if (!evaluate_job(bundle, in, o, tr_, err) || o.real.total_hours.is_zero()) continue;
            c.points.append(QPointF(o.hourly_wage.to_double(), o.real.real_hourly.to_double()));
        }
        c.marker = QPointF(r.emp.hourly_wage.to_double(), r.realHourly);
        if (c.points.size() > 1) curves.append(c);
        curve_cur = curve_cur.isEmpty() ? r.currency : (curve_cur == r.currency ? curve_cur : QStringLiteral("mixed"));
    }
    curveChart_->setCurves(curves, curve_cur == "mixed" ? tr("currencies differ") : curve_cur);

    // Break-even.
    breakEven_->clear();
    futureText_->clear();
    const JobResult* a = nullptr;
    const JobResult* b = nullptr;
    for (const JobResult& r : results_) {
        if (r.job.id == adjustJob_->currentData().toString()) a = &r;
        if (r.job.id == matchJob_->currentData().toString()) b = &r;
    }
    if (!a || !b || a == b || !a->ok || !b->ok || !b->hasHourly) {
        breakEven_->setText(results_.size() < 2 ? tr("Add a second job to ask what pay would make one match the other.") : tr("Pick two different jobs."));
    } else if (a->currency != b->currency) {
        breakEven_->setText(tr("These jobs are paid in different currencies, so they cannot be matched."));
    } else if (auto bundle = lookup_(a->job.packId)) {
        const JobInputs in = inputsFromState(a->job.state);
        const bool salary = in.basis == PayBasis::Salary;
        const QString unit = salary ? tr("a year") : tr("an hour");
        const Decimal ceiling = in.pay.is_zero() ? Decimal::from_int(salary ? 2000000 : 1000) : in.pay * Decimal::from_int(20);
        Decimal need;
        std::string err;
        if (a->hasHourly && a->realHourly >= b->realHourly - 1e-9) {
            breakEven_->setText(tr("<b>%1</b> already gives you %2 per hour of your time, at least as much as %3 (%4). Nothing to change.")
                                    .arg(a->job.name.toHtmlEscaped(), money(a->realHourly, a->currency), b->job.name.toHtmlEscaped(), money(b->realHourly, b->currency)));
        } else if (solve_pay_for_real_hourly(bundle, in, D(b->realHourly), ceiling, need, err)) {
            const double n = need.to_double(), cur = in.pay.to_double();
            breakEven_->setText(
                tr("<b>%1</b> would need to pay <b>%2 %3</b> to match <b>%4</b> (%5 real hourly). It pays %6 now, so that is %7% more.")
                    .arg(a->job.name.toHtmlEscaped(), money(n, a->currency), unit, b->job.name.toHtmlEscaped(), money(b->realHourly, b->currency),
                         money(cur, a->currency), cur > 0 ? QString::number((n / cur - 1) * 100.0, 'f', 1) : QStringLiteral("–")));
        } else {
            breakEven_->setText(tr("Even at 20 times its current pay, <b>%1</b> would not match <b>%2</b> (its costs and hours are the problem, not the pay).")
                                    .arg(a->job.name.toHtmlEscaped(), b->job.name.toHtmlEscaped()));
        }
    }

    // Pay in N years, for the job being adjusted.
    if (a && a->ok && !a->notWorking) {
        auto bundle = lookup_(a->job.packId);
        const double raise = a->job.state.value("raise").toDouble();
        const int n = years_->value();
        if (bundle) {
            JobInputs in = inputsFromState(a->job.state);
            in.pay = in.pay * D(std::pow(1.0 + raise / 100.0, n));
            JobOutcome o;
            std::vector<TraceEntry> tr_;
            std::string err;
            if (evaluate_job(bundle, in, o, tr_, err) && !o.real.total_hours.is_zero()) {
                const bool salary = in.basis == PayBasis::Salary;
                futureText_->setText(tr("With %1% raises, <b>%2</b> would pay %3 %4 after %5 years, and about <b>%6</b> real hourly (today %7). Tax brackets and costs are held at today's values, so treat this as a rough guide.")
                                         .arg(raise, 0, 'f', 1)
                                         .arg(a->job.name.toHtmlEscaped(), money(in.pay.to_double(), a->currency), salary ? tr("a year") : tr("an hour"))
                                         .arg(n)
                                         .arg(money(o.real.real_hourly.to_double(), a->currency), money(a->realHourly, a->currency)));
            }
        }
    }
}

void ComparePage::showSubTab(int sub, int chart) {
    tabs_->setCurrentIndex(sub);
    chartKind_->setCurrentIndex(chart);
}
