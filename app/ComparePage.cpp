#include "ComparePage.hpp"

#include <QAction>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
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

    // ---- What if
    auto* wi = new QWidget;
    auto* wl = new QVBoxLayout(wi);
    auto* pick = new QHBoxLayout;
    pick->addWidget(new QLabel(tr("Job to explore:")));
    adjustJob_ = new QComboBox;
    adjustJob_->setMinimumWidth(240);
    pick->addWidget(adjustJob_);
    pick->addStretch(1);
    wl->addLayout(pick);
    whatTabs_ = new QTabWidget;
    wl->addWidget(whatTabs_, 1);

    // Scenario: change things and see the result next to today's figures.
    auto* sc = new QWidget;
    auto* scl = new QHBoxLayout(sc);
    auto* form = new QFormLayout;
    auto spin = [&](const QString& label, double lo, double hi, double step, int dec, const QString& suffix, const QString& tip) {
        auto* d = new QDoubleSpinBox;
        d->setRange(lo, hi);
        d->setSingleStep(step);
        d->setDecimals(dec);
        d->setSuffix(suffix);
        d->setToolTip(tip);
        d->setMinimumWidth(130);
        d->setKeyboardTracking(false);
        form->addRow(label, d);
        connect(d, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
            if (!updating_) buildWhatIf();
        });
        return d;
    };
    sPay_ = spin(tr("Change the pay by"), -90, 300, 1, 1, " %", tr("A one-off change to the wage or salary, for example a negotiated offer. 10 means 10% more."));
    sRaise_ = spin(tr("Then yearly raises of"), -10, 30, 0.5, 1, " % a year", tr("Starts from the raise on the job's own page. Applied once per year, for the years below."));
    sYears_ = spin(tr("…for"), 0, 40, 1, 0, " years", tr("How far ahead to look. 0 shows only the one-off changes."));
    sInflation_ = spin(tr("Prices rise by"), 0, 25, 0.5, 1, " % a year", tr("Used only to show the result in today's money. 0 hides it."));
    sHours_ = spin(tr("Change weekly hours by"), -60, 60, 1, 1, " h", tr("Negative for fewer hours. Overtime rules apply to the new total for hourly jobs; a salary stays the same."));
    sCommute_ = spin(tr("Change one-way commute by"), -120, 120, 5, 0, " min", tr("Per trip. Cost per kilometre or mile, parking and fares are unchanged."));
    sRemote_ = spin(tr("Change days worked from home by"), -5, 5, 1, 0, " days", tr("Each day at home saves that day's commute time and costs."));
    sBonus_ = spin(tr("Change bonus by"), -100000, 100000, 500, 0, " /year", tr("Taxable, per year."));
    sBenefits_ = spin(tr("Change benefits by"), -5000, 5000, 25, 0, " /month", tr("Value to you of employer-paid benefits, per month."));
    sCosts_ = spin(tr("Change work costs by"), -50000, 50000, 100, 0, " /year", tr("Tools, uniforms, courses and the like, per year."));
    sVacation_ = spin(tr("Change paid vacation by"), -10, 10, 1, 1, " weeks", tr("More paid vacation means fewer days worked for the same pay."));
    auto* reset = new QPushButton(tr("Reset all"));
    connect(reset, &QPushButton::clicked, this, [this]() {
        updating_ = true;
        for (auto* d : {sPay_, sYears_, sInflation_, sHours_, sCommute_, sRemote_, sBonus_, sBenefits_, sCosts_, sVacation_}) d->setValue(0);
        updating_ = false;
        loadScenarioDefaults();
        buildWhatIf();
    });
    form->addRow(reset);
    auto* formHost = new QWidget;
    formHost->setLayout(form);
    auto* formScroll = new QScrollArea;
    formScroll->setWidget(formHost);
    formScroll->setWidgetResizable(true);
    formScroll->setFixedWidth(430);
    scl->addWidget(formScroll);
    auto* scRight = new QVBoxLayout;
    scenarioText_ = new QLabel;
    scenarioText_->setWordWrap(true);
    scenarioText_->setTextFormat(Qt::RichText);
    scRight->addWidget(scenarioText_);
    scenarioTable_ = new QTableWidget;
    scenarioTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    scenarioTable_->setSelectionMode(QAbstractItemView::NoSelection);
    scenarioTable_->setShowGrid(false);
    scenarioTable_->setAlternatingRowColors(true);
    scenarioTable_->setColumnCount(3);
    scenarioTable_->setHorizontalHeaderLabels({tr("Today"), tr("With your changes"), tr("Difference")});
    scenarioTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    scenarioTable_->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    scRight->addWidget(scenarioTable_, 1);
    auto* scNote = new QLabel(tr("Tax brackets, costs per kilometre, fares and parking are held at today's values; only what you change above moves."));
    scNote->setObjectName("muted");
    scNote->setWordWrap(true);
    scRight->addWidget(scNote);
    scl->addLayout(scRight, 1);
    whatTabs_->addTab(sc, tr("Scenario"));

    // Break-even: what pay is needed to match something.
    auto* be = new QWidget;
    auto* bel = new QVBoxLayout(be);
    auto* beForm = new QFormLayout;
    matchJob_ = new QComboBox;
    beForm->addRow(tr("What pay would make the job as good per hour of your time as:"), matchJob_);
    target_ = new QDoubleSpinBox;
    target_->setRange(0, 10000);
    target_->setDecimals(2);
    target_->setValue(25);
    target_->setSuffix(tr(" real hourly"));
    target_->setKeyboardTracking(false);
    beForm->addRow(tr("A figure of your own:"), target_);
    bel->addLayout(beForm);
    breakEven_ = new QLabel;
    breakEven_->setWordWrap(true);
    breakEven_->setTextFormat(Qt::RichText);
    bel->addWidget(breakEven_);
    auto* beNote = new QLabel(tr("The answer keeps everything you changed on the Scenario tab (hours, commute, benefits…) and solves only for the pay."));
    beNote->setObjectName("muted");
    beNote->setWordWrap(true);
    bel->addWidget(beNote);
    bel->addStretch(1);
    whatTabs_->addTab(be, tr("Break-even"));

    // Sensitivity: which assumptions matter most.
    auto* se = new QWidget;
    auto* sel = new QVBoxLayout(se);
    auto* seNote = new QLabel(tr("Each line changes one thing about the job, alone, and shows what that does to your real hourly pay. The biggest effects are first, so you can see which numbers deserve a second look."));
    seNote->setWordWrap(true);
    seNote->setObjectName("muted");
    sel->addWidget(seNote);
    sensTable_ = new QTableWidget;
    sensTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    sensTable_->setSelectionMode(QAbstractItemView::NoSelection);
    sensTable_->setShowGrid(false);
    sensTable_->setAlternatingRowColors(true);
    sensTable_->setColumnCount(3);
    sensTable_->setHorizontalHeaderLabels({tr("If this changes"), tr("Real hourly becomes"), tr("Effect")});
    sensTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    sensTable_->verticalHeader()->hide();
    sel->addWidget(sensTable_, 1);
    whatTabs_->addTab(se, tr("What matters most"));

    // Curves.
    auto* cv = new QWidget;
    auto* cvl = new QVBoxLayout(cv);
    auto* curveNote = new QLabel(tr("Pay curves: how each job's real hourly changes if its pay were lower or higher (50% to 200% of what you entered). Taxes rise faster than pay, so the curve flattens. Dots mark today's pay."));
    curveNote->setWordWrap(true);
    curveNote->setObjectName("muted");
    cvl->addWidget(curveNote);
    curveChart_ = new ChartWidget;
    curveChart_->setMode(ChartWidget::PayCurve);
    cvl->addWidget(curveChart_, 1);
    whatTabs_->addTab(cv, tr("Pay curves"));
    tabs_->addTab(wi, tr("What if"));
    connect(adjustJob_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (updating_) return;
        loadScenarioDefaults();
        buildWhatIf();
    });
    connect(matchJob_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        target_->setEnabled(matchJob_->currentData().toString().isEmpty());
        if (!updating_) buildWhatIf();
    });
    connect(target_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
        if (!updating_) buildWhatIf();
    });
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
    {
        const QString keep = matchJob_->currentData().toString();
        matchJob_->clear();
        for (const JobResult& r : results_)
            if (r.ok && !r.notWorking && r.job.id != adjustJob_->currentData().toString()) matchJob_->addItem(r.job.name, r.job.id);
        matchJob_->addItem(tr("A figure I choose…"), QString());
        const int i = matchJob_->findData(keep);
        matchJob_->setCurrentIndex(i >= 0 ? i : 0);
        target_->setEnabled(matchJob_->currentData().toString().isEmpty());
    }
    if (firstLoad_ && adjustJob_->count() > 0) {
        firstLoad_ = false;
        loadScenarioDefaults();
    }
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

namespace {

struct Scenario {
    double pay = 0, raise = 0, years = 0, hours = 0, commute = 0, remote = 0, bonus = 0, benefits = 0, costs = 0, vacation = 0;
};

JobInputs applyScenario(JobInputs in, const Scenario& s, bool includePay = true) {
    auto clampNonNeg = [](Decimal d) { return d.is_negative() ? Decimal() : d; };
    if (includePay) in.pay = clampNonNeg(in.pay * D((1.0 + s.pay / 100.0) * std::pow(1.0 + s.raise / 100.0, s.years)));
    in.hours_per_week = clampNonNeg(in.hours_per_week + D(s.hours));
    if (in.hours_per_week > Decimal::from_int(168)) in.hours_per_week = Decimal::from_int(168);
    in.commute_minutes = clampNonNeg(in.commute_minutes + D(s.commute));
    in.remote_days_per_week = clampNonNeg(in.remote_days_per_week + D(s.remote));
    if (in.remote_days_per_week > in.days_per_week) in.remote_days_per_week = in.days_per_week;
    in.bonus_annual = clampNonNeg(in.bonus_annual + D(s.bonus));
    in.benefits_monthly_value = clampNonNeg(in.benefits_monthly_value + D(s.benefits));
    in.work_costs_annual = clampNonNeg(in.work_costs_annual + D(s.costs));
    in.paid_vacation_weeks = clampNonNeg(in.paid_vacation_weeks + D(s.vacation));
    if (in.paid_vacation_weeks > in.paid_weeks) in.paid_vacation_weeks = in.paid_weeks;
    return in;
}

bool evalJob(const std::shared_ptr<const Bundle>& b, const JobInputs& in, JobOutcome& o, std::string& err) {
    std::vector<TraceEntry> t;
    return evaluate_job(b, in, o, t, err);
}

}  // namespace

void ComparePage::loadScenarioDefaults() {
    updating_ = true;
    for (const JobResult& r : results_)
        if (r.job.id == adjustJob_->currentData().toString()) sRaise_->setValue(r.job.state.value("raise").toDouble());
    updating_ = false;
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
            std::string err;
            if (!evalJob(bundle, in, o, err) || o.real.total_hours.is_zero()) continue;
            c.points.append(QPointF(o.hourly_wage.to_double(), o.real.real_hourly.to_double()));
        }
        c.marker = QPointF(r.emp.hourly_wage.to_double(), r.realHourly);
        if (c.points.size() > 1) curves.append(c);
        curve_cur = curve_cur.isEmpty() ? r.currency : (curve_cur == r.currency ? curve_cur : QStringLiteral("mixed"));
    }
    curveChart_->setCurves(curves, curve_cur == "mixed" ? tr("currencies differ") : curve_cur);

    breakEven_->clear();
    scenarioText_->clear();
    scenarioTable_->setRowCount(0);
    sensTable_->setRowCount(0);
    const JobResult* a = nullptr;
    for (const JobResult& r : results_)
        if (r.job.id == adjustJob_->currentData().toString()) a = &r;
    if (!a || !a->ok || a->notWorking) {
        scenarioText_->setText(results_.isEmpty() ? tr("Add a job to explore it here.") : tr("Pick a job above. (Not-working situations have no pay to explore.)"));
        return;
    }
    auto bundle = lookup_(a->job.packId);
    if (!bundle) return;
    const QString cur = a->currency;
    const JobInputs base = inputsFromState(a->job.state);
    const bool salary = base.basis == PayBasis::Salary;

    Scenario sc;
    sc.pay = sPay_->value();
    sc.raise = sRaise_->value();
    sc.years = sYears_->value();
    sc.hours = sHours_->value();
    sc.commute = sCommute_->value();
    sc.remote = sRemote_->value();
    sc.bonus = sBonus_->value();
    sc.benefits = sBenefits_->value();
    sc.costs = sCosts_->value();
    sc.vacation = sVacation_->value();
    const JobInputs scen = applyScenario(base, sc);

    // ---- Scenario
    JobOutcome now, then;
    std::string err;
    if (!evalJob(bundle, base, now, err) || !evalJob(bundle, scen, then, err)) {
        scenarioText_->setText(tr("These changes cannot be calculated: %1").arg(QString::fromStdString(err)));
    } else {
        struct R { QString label; double a, b; bool money; int dec; };
        const double infl = std::pow(1.0 + sInflation_->value() / 100.0, sc.years);
        QList<R> rowsv = {
            {salary ? tr("Salary per year") : tr("Hourly wage"), base.pay.to_double(), scen.pay.to_double(), true, 2},
            {tr("Gross pay per year"), now.take_home.gross_annual.to_double(), then.take_home.gross_annual.to_double(), true, 0},
            {tr("Take-home per month"), now.take_home.net_monthly.to_double(), then.take_home.net_monthly.to_double(), true, 0},
            {tr("Net value per year"), now.real.value_annual.to_double(), then.real.value_annual.to_double(), true, 0},
            {tr("Hours the job takes, per week"), now.real.total_hours.to_double() / std::max(1.0, base.paid_weeks.to_double()),
             then.real.total_hours.to_double() / std::max(1.0, scen.paid_weeks.to_double()), false, 1},
            {tr("Real hourly"), now.real.real_hourly.to_double(), then.real.real_hourly.to_double(), true, 2},
        };
        if (infl > 1.0000001) rowsv.append({tr("Real hourly in today's money"), now.real.real_hourly.to_double(), then.real.real_hourly.to_double() / infl, true, 2});
        scenarioTable_->setRowCount(rowsv.size());
        QStringList heads;
        for (int i = 0; i < rowsv.size(); ++i) {
            const R& r = rowsv[i];
            heads << r.label;
            auto fmt = [&](double v) { return QLocale().toString(v, 'f', r.dec) + (r.money ? " " + cur : QString()); };
            const double d = r.b - r.a;
            auto put = [&](int col, const QString& t, bool bold = false, QColor fg = QColor()) {
                auto* it = new QTableWidgetItem(t);
                it->setTextAlignment(Qt::AlignCenter);
                QFont f = it->font();
                f.setBold(bold);
                it->setFont(f);
                if (fg.isValid()) it->setForeground(fg);
                scenarioTable_->setItem(i, col, it);
            };
            put(0, fmt(r.a));
            put(1, fmt(r.b), true);
            const bool timeRow = r.label == tr("Hours the job takes, per week");
            const bool good = timeRow ? d < 0 : d > 0;
            put(2, std::abs(d) < 0.005 ? QStringLiteral("–") : (d > 0 ? "+" : "") + fmt(d), false,
                std::abs(d) < 0.005 ? QColor() : good ? QColor("#26a269") : QColor("#c01c1c"));
        }
        scenarioTable_->setVerticalHeaderLabels(heads);
        const double r0 = now.real.real_hourly.to_double(), r1 = then.real.real_hourly.to_double();
        QString t = tr("With these changes <b>%1</b> pays <b>%2</b> per hour of your time, ").arg(a->job.name.toHtmlEscaped(), money(r1, cur));
        t += std::abs(r1 - r0) < 0.005 ? tr("the same as today.") : tr("%1 %2 than today (%3).").arg(money(std::abs(r1 - r0), cur), r1 > r0 ? tr("more") : tr("less"), money(r0, cur));
        if (sc.years > 0 && std::abs(sc.raise) > 0.001)
            t += " " + tr("Pay would be %1 %2 after %3 years of %4% raises.").arg(money(scen.pay.to_double(), cur), salary ? tr("a year") : tr("an hour")).arg(int(sc.years)).arg(sc.raise, 0, 'f', 1);
        scenarioText_->setText(t);
    }

    // ---- Break-even (on the scenario, solving for pay)
    const QString matchId = matchJob_->currentData().toString();
    const JobResult* b = nullptr;
    for (const JobResult& r : results_)
        if (r.job.id == matchId) b = &r;
    double targetValue = 0;
    QString targetName;
    bool haveTarget = false;
    if (matchId.isEmpty()) {
        targetValue = target_->value();
        targetName = tr("your figure of %1").arg(money(targetValue, cur));
        haveTarget = true;
    } else if (b && b != a && b->ok && b->hasHourly) {
        if (b->currency != cur) {
            breakEven_->setText(tr("These jobs are paid in different currencies, so they cannot be matched."));
        } else {
            targetValue = b->realHourly;
            targetName = tr("<b>%1</b> (%2 real hourly)").arg(b->job.name.toHtmlEscaped(), money(targetValue, cur));
            haveTarget = true;
        }
    } else {
        breakEven_->setText(results_.size() < 2 ? tr("Add a second job to ask what pay would make one match the other, or choose “A figure I choose”.") : tr("Pick something to match."));
    }
    if (haveTarget) {
        const JobInputs withoutPay = applyScenario(base, sc, false);
        JobOutcome cur0;
        const bool okNow = evalJob(bundle, scen, cur0, err);
        Decimal need;
        const Decimal ceiling = withoutPay.pay.is_zero() ? Decimal::from_int(salary ? 2000000 : 1000) : withoutPay.pay * Decimal::from_int(20);
        const QString unit = salary ? tr("a year") : tr("an hour");
        if (okNow && cur0.real.real_hourly.to_double() >= targetValue - 1e-9) {
            breakEven_->setText(tr("<b>%1</b>, with your changes, already gives %2 per hour of your time, at least as much as %3. Nothing needs to change.")
                                    .arg(a->job.name.toHtmlEscaped(), money(cur0.real.real_hourly.to_double(), cur), targetName));
        } else if (solve_pay_for_real_hourly(bundle, withoutPay, D(targetValue), ceiling, need, err)) {
            const double n = need.to_double(), have = scen.pay.to_double();
            breakEven_->setText(tr("<b>%1</b> would need to pay <b>%2 %3</b> to match %4. With your changes it pays %5, so that is %6% more.")
                                    .arg(a->job.name.toHtmlEscaped(), money(n, cur), unit, targetName, money(have, cur),
                                         have > 0 ? QString::number((n / have - 1) * 100.0, 'f', 1) : QStringLiteral("–")));
        } else {
            breakEven_->setText(tr("Even at 20 times its pay, <b>%1</b> would not match %2: its hours, commute and costs are the problem, not the pay. Try changing those on the Scenario tab.")
                                    .arg(a->job.name.toHtmlEscaped(), targetName));
        }
    }

    // ---- Sensitivity (one change at a time, from today's inputs)
    struct Probe { QString label; Scenario s; };
    QList<Probe> probes;
    auto add = [&](const QString& label, Scenario s) { probes.append({label, s}); };
    { Scenario s; s.pay = 10; add(tr("Pay 10% higher"), s); }
    { Scenario s; s.pay = -10; add(tr("Pay 10% lower"), s); }
    { Scenario s; s.hours = 5; add(tr("5 more hours a week"), s); }
    { Scenario s; s.hours = -5; add(tr("5 fewer hours a week"), s); }
    { Scenario s; s.commute = 15; add(tr("Commute 15 minutes longer each way"), s); }
    { Scenario s; s.commute = -15; add(tr("Commute 15 minutes shorter each way"), s); }
    { Scenario s; s.remote = 1; add(tr("One more day from home"), s); }
    { Scenario s; s.benefits = 100; add(tr("Benefits worth 100 more a month"), s); }
    { Scenario s; s.bonus = 2000; add(tr("2,000 more bonus a year"), s); }
    { Scenario s; s.costs = 1000; add(tr("1,000 more work costs a year"), s); }
    { Scenario s; s.vacation = 1; add(tr("One more week of paid vacation"), s); }
    struct Out { QString label; double value, delta; };
    QList<Out> outs;
    const double r0 = now.real.real_hourly.to_double();
    for (const Probe& p : probes) {
        JobOutcome o;
        if (!evalJob(bundle, applyScenario(base, p.s), o, err) || o.real.total_hours.is_zero()) continue;
        outs.append({p.label, o.real.real_hourly.to_double(), o.real.real_hourly.to_double() - r0});
    }
    std::sort(outs.begin(), outs.end(), [](const Out& x, const Out& y) { return std::abs(x.delta) > std::abs(y.delta); });
    sensTable_->setRowCount(outs.size());
    for (int i = 0; i < outs.size(); ++i) {
        auto put = [&](int col, const QString& t, QColor fg = QColor(), bool left = false) {
            auto* it = new QTableWidgetItem(t);
            it->setTextAlignment(left ? Qt::AlignLeft | Qt::AlignVCenter : Qt::AlignCenter);
            if (fg.isValid()) it->setForeground(fg);
            sensTable_->setItem(i, col, it);
        };
        put(0, outs[i].label, QColor(), true);
        put(1, money(outs[i].value, cur));
        put(2, (outs[i].delta >= 0 ? "+" : "") + money(outs[i].delta, cur), outs[i].delta >= 0 ? QColor("#26a269") : QColor("#c01c1c"));
    }
}

void ComparePage::showSubTab(int sub, int chart) {
    tabs_->setCurrentIndex(sub);
    if (sub == 3) whatTabs_->setCurrentIndex(chart);
    else chartKind_->setCurrentIndex(chart);
}

void ComparePage::setScenario(const QVariantMap& v) {
    updating_ = true;
    const QMap<QString, QDoubleSpinBox*> m = {{"pay", sPay_}, {"raise", sRaise_}, {"years", sYears_}, {"inflation", sInflation_}, {"hours", sHours_},
                                              {"commute", sCommute_}, {"remote", sRemote_}, {"bonus", sBonus_}, {"benefits", sBenefits_},
                                              {"costs", sCosts_}, {"vacation", sVacation_}};
    for (auto it = v.begin(); it != v.end(); ++it)
        if (m.contains(it.key())) m[it.key()]->setValue(it.value().toDouble());
    updating_ = false;
    buildWhatIf();
}
