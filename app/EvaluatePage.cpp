#include "EvaluatePage.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QVBoxLayout>

#include "Format.hpp"
#include "evalcore/realhourly.hpp"
#include "evalcore/takehome.hpp"

using namespace jobeval;

namespace {

Decimal dec(const QDoubleSpinBox* sb) {
    return Decimal::parse(QString::number(sb->value(), 'f', sb->decimals()).toStdString());
}

void bigNumber(const QString& caption, QLabel*& valueOut, QHBoxLayout* row, bool highlight) {
    auto* card = new QWidget;
    card->setObjectName(highlight ? "cardHot" : "card");
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(16, 12, 16, 12);
    v->setSpacing(2);
    auto* cap = new QLabel(caption);
    cap->setObjectName(highlight ? "cardHotCaption" : "muted");
    valueOut = new QLabel("–");
    valueOut->setObjectName(highlight ? "bigNumberHot" : "bigNumber");
    v->addWidget(cap);
    v->addWidget(valueOut);
    row->addWidget(card, 1);
}

QTableWidget* makeTable(const QStringList& headers) {
    auto* t = new QTableWidget(0, headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int i = 1; i < headers.size(); ++i) t->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    t->verticalHeader()->hide();
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->setShowGrid(false);
    t->setAlternatingRowColors(true);
    t->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    t->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    return t;
}

struct Row {
    QString label;
    QString a;
    QString b;
    bool bold = false;
    int indent = 0;
};

void fillTable(QTableWidget* t, const std::vector<Row>& rows) {
    t->setRowCount(static_cast<int>(rows.size()));
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        const Row& row = rows[static_cast<size_t>(i)];
        QTableWidgetItem* items[3] = {new QTableWidgetItem(QString(row.indent * 4, ' ') + row.label),
                                      new QTableWidgetItem(row.a), new QTableWidgetItem(row.b)};
        for (int c = 0; c < t->columnCount(); ++c) {
            QFont f = items[c]->font();
            f.setBold(row.bold);
            items[c]->setFont(f);
            if (c > 0) items[c]->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            if (row.indent > 0) items[c]->setForeground(t->palette().color(QPalette::PlaceholderText));
            t->setItem(i, c, items[c]);
        }
    }
    t->resizeRowsToContents();
    int h = t->horizontalHeader()->height() + 4;
    for (int i = 0; i < t->rowCount(); ++i) h += t->rowHeight(i);
    t->setFixedHeight(h);
}

}  // namespace

QDoubleSpinBox* EvaluatePage::addField(QFormLayout* form, const QString& key, const QString& label, double min,
                                       double max, int decimals, double value, const QString& suffix, const QString& tip) {
    auto* s = new QDoubleSpinBox;
    s->setRange(min, max);
    s->setDecimals(decimals);
    s->setValue(value);
    s->setSuffix(suffix);
    s->setGroupSeparatorShown(true);
    s->setMinimumWidth(120);
    s->setKeyboardTracking(false);
    if (!tip.isEmpty()) s->setToolTip(tip);
    fields_[key] = s;
    form->addRow(label, s);
    connect(s, &QDoubleSpinBox::valueChanged, this, [this]() {
        recalc();
        Q_EMIT inputsChanged();
    });
    return s;
}

EvaluatePage::EvaluatePage(QWidget* parent) : QWidget(parent) {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(20);

    // ---- left: inputs, scrollable
    auto* leftInner = new QWidget;
    auto* left = new QVBoxLayout(leftInner);
    left->setContentsMargins(0, 0, 8, 0);
    regionTitle_ = new QLabel;
    regionTitle_->setObjectName("pageTitle");
    regionTitle_->setWordWrap(true);
    auto* change = new QPushButton(tr("Change region"));
    change->setFlat(true);
    connect(change, &QPushButton::clicked, this, &EvaluatePage::changeRegionRequested);
    left->addWidget(regionTitle_);
    left->addWidget(change, 0, Qt::AlignLeft);

    auto* jobBox = new QGroupBox(tr("The job"));
    auto* job = new QFormLayout(jobBox);
    addField(job, "wage", tr("Hourly wage"), 0, 9999.99, 2, 20.0, QString());
    addField(job, "hours", tr("Paid hours per week"), 0, 168, 1, 40.0, tr(" h"));
    addField(job, "weeks", tr("Paid weeks per year"), 0, 52, 0, 52, tr(" wk"));
    addField(job, "vacation", tr("…of which paid vacation"), 0, 52, 0, 0, tr(" wk"),
             tr("Paid weeks in which you do not work: no commute, no hours worked."));
    addField(job, "days", tr("Days worked per week"), 0, 7, 1, 5, QString());
    overtime_ = new QCheckBox(tr("Hours over the legal threshold earn overtime"));
    overtime_->setChecked(true);
    overtime_->setToolTip(tr("Uses the region's legal minimum overtime rate. Untick for salaried or exempt work."));
    job->addRow(overtime_);
    connect(overtime_, &QCheckBox::toggled, this, [this]() {
        recalc();
        Q_EMIT inputsChanged();
    });
    left->addWidget(jobBox);

    auto* goBox = new QGroupBox(tr("Getting there (one way)"));
    auto* go = new QFormLayout(goBox);
    unit_ = new QComboBox;
    unit_->addItems({"km", "mi"});
    go->addRow(tr("Distance unit"), unit_);
    addField(go, "distance", tr("Distance"), 0, 999, 1, 0, QString());
    addField(go, "minutes", tr("Door-to-door time"), 0, 600, 0, 0, tr(" min"));
    addField(go, "costper", tr("Vehicle cost per km"), 0, 99, 3, 0, QString(),
             tr("All-in cost of driving one unit: fuel, maintenance, tyres, insurance share and depreciation. "
                "Tax authorities publish per-distance rates that make a reasonable guide."));
    addField(go, "parking", tr("Parking and tolls per day"), 0, 999, 2, 0, QString());
    addField(go, "transit", tr("Transit fare per day"), 0, 999, 2, 0, QString());
    left->addWidget(goBox);
    connect(unit_, &QComboBox::currentIndexChanged, this, [this]() {
        updateUnitLabels();
        Q_EMIT inputsChanged();
    });

    auto* extraCostBox = new QGroupBox(tr("Benefits and costs"));
    auto* ec = new QFormLayout(extraCostBox);
    addField(ec, "benefits", tr("Employer benefits, per month"), 0, 99999, 2, 0, QString(),
             tr("Cash value of employer-paid health, dental, insurance and similar."));
    addField(ec, "pension", tr("Pension / RRSP match"), 0, 100, 1, 0, tr(" %"), tr("Employer contribution as a percent of gross pay."));
    addField(ec, "otherben", tr("Other benefits, per year"), 0, 9999999, 0, 0, QString());
    addField(ec, "workcosts", tr("Work costs you pay, per year"), 0, 9999999, 0, 0, QString(),
             tr("Uniforms, tools, phone, licences and similar."));
    addField(ec, "unpaid", tr("Unpaid time per day"), 0, 600, 0, 0, tr(" min"), tr("Unpaid breaks, changing, waiting."));
    left->addWidget(extraCostBox);

    extraBox_ = new QGroupBox(tr("Region-specific"));
    extraForm_ = new QFormLayout(extraBox_);
    extraBox_->hide();
    left->addWidget(extraBox_);

    auto* dateBox = new QGroupBox(tr("Rules"));
    auto* dform = new QFormLayout(dateBox);
    asOf_ = new QDateEdit(QDate::currentDate());
    asOf_->setCalendarPopup(true);
    dform->addRow(tr("Rules as of"), asOf_);
    left->addWidget(dateBox);
    left->addStretch(1);

    auto* scroll = new QScrollArea;
    scroll->setWidget(leftInner);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFixedWidth(350);
    root->addWidget(scroll);

    // ---- right: results, scrollable
    auto* rightInner = new QWidget;
    auto* right = new QVBoxLayout(rightInner);
    right->setContentsMargins(0, 0, 8, 0);
    auto* cards = new QHBoxLayout;
    bigNumber(tr("Real hourly"), realHourly_, cards, true);
    bigNumber(tr("Take-home per month"), monthly_, cards, false);
    bigNumber(tr("Per paid hour, after tax"), hourlyAfterTax_, cards, false);
    right->addLayout(cards);

    verdict_ = new QLabel;
    verdict_->setWordWrap(true);
    verdict_->setObjectName("muted");
    right->addWidget(verdict_);

    error_ = new QLabel;
    error_->setObjectName("error");
    error_->setWordWrap(true);
    error_->hide();
    right->addWidget(error_);

    auto* h1 = new QLabel(tr("Money, per year"));
    h1->setObjectName("sectionTitle");
    right->addWidget(h1);
    money_ = makeTable({tr("Item"), tr("Per year"), tr("Per month")});
    right->addWidget(money_);

    auto* h2 = new QLabel(tr("Your time, per year"));
    h2->setObjectName("sectionTitle");
    right->addWidget(h2);
    time_ = makeTable({tr("Item"), tr("Hours"), tr("Per week")});
    right->addWidget(time_);

    coverage_ = new QLabel;
    coverage_->setWordWrap(true);
    coverage_->setObjectName("muted");
    right->addWidget(coverage_);

    auto* explainBox = new QGroupBox(tr("How the tax was calculated"));
    explainBox->setCheckable(true);
    explainBox->setChecked(false);
    auto* ev = new QVBoxLayout(explainBox);
    explain_ = new QPlainTextEdit;
    explain_->setReadOnly(true);
    explain_->setMinimumHeight(180);
    explain_->setFont(QFont("monospace"));
    ev->addWidget(explain_);
    explain_->hide();
    connect(explainBox, &QGroupBox::toggled, explain_, &QWidget::setVisible);
    right->addWidget(explainBox);
    right->addStretch(1);

    auto* rscroll = new QScrollArea;
    rscroll->setWidget(rightInner);
    rscroll->setWidgetResizable(true);
    rscroll->setFrameShape(QFrame::NoFrame);
    rscroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rscroll->setMinimumWidth(560);
    root->addWidget(rscroll, 1);

    connect(asOf_, &QDateEdit::dateChanged, this, [this]() { recalc(); });
    updateUnitLabels();
}

void EvaluatePage::updateUnitLabels() {
    const QString u = unit_->currentText();
    for (auto* w : findChildren<QFormLayout*>()) {
        if (auto* lbl = qobject_cast<QLabel*>(w->labelForField(fields_["costper"]))) lbl->setText(tr("Vehicle cost per %1").arg(u));
        if (auto* lbl = qobject_cast<QLabel*>(w->labelForField(fields_["distance"]))) lbl->setText(tr("Distance (%1)").arg(u));
    }
    recalc();
}

QVariantMap EvaluatePage::state() const {
    QVariantMap m;
    for (auto it = fields_.begin(); it != fields_.end(); ++it) m[it.key()] = it.value()->value();
    m["overtime"] = overtime_->isChecked();
    m["unit"] = unit_->currentIndex();
    return m;
}

void EvaluatePage::setState(const QVariantMap& values) {
    for (auto it = fields_.begin(); it != fields_.end(); ++it) it.value()->blockSignals(true);
    for (auto it = values.begin(); it != values.end(); ++it) {
        if (fields_.contains(it.key())) fields_[it.key()]->setValue(it.value().toDouble());
    }
    for (auto it = fields_.begin(); it != fields_.end(); ++it) it.value()->blockSignals(false);
    if (values.contains("overtime")) overtime_->setChecked(values["overtime"].toBool());
    if (values.contains("unit")) unit_->setCurrentIndex(values["unit"].toInt());
    updateUnitLabels();
}

void EvaluatePage::setRegion(const std::shared_ptr<const Bundle>& bundle, const CatalogEntry* entry, const QString& title) {
    bundle_ = bundle;
    regionTitle_->setText(title);
    const bool hasOvertime = bundle->slots.count("overtime_rate") && bundle->slots.count("overtime_threshold_hours");
    overtime_->setVisible(hasOvertime);

    const QString tier = entry ? QString::fromStdString(entry->tier) : QString::fromStdString(bundle->tier);
    QString cov = tier == "full" ? tr("Full coverage.") : tier == "partial" ? tr("Partial coverage.") : tr("Rough estimate.");
    if (!bundle->not_modelled.empty()) {
        QStringList items;
        for (const auto& n : bundle->not_modelled) {
            QString s = QString::fromStdString(n);
            const int colon = s.indexOf(": ");
            if (colon >= 0) s = s.mid(colon + 2);
            items << s.replace('_', ' ');
        }
        cov += " " + tr("Not included in the tax estimate: %1.").arg(items.join(", "));
    }
    if (entry && !entry->last_verified.empty()) cov += " " + tr("Rules last verified %1.").arg(QString::fromStdString(entry->last_verified));
    cov += " " + tr("Tax is computed as if this were your only income for a full year. These are estimates for "
                    "comparing jobs, not payroll-grade figures.");
    coverage_->setText(cov);
    buildExtraInputs();
    recalc();
}

void EvaluatePage::buildExtraInputs() {
    while (extraForm_->rowCount() > 0) extraForm_->removeRow(0);
    extras_.clear();
    static const QStringList standard = {"gross_annual", "wage"};
    for (const auto& kv : bundle_->inputs) {
        const QString name = QString::fromStdString(kv.first);
        if (standard.contains(name)) continue;
        const InputDef& in = kv.second;
        auto* s = new QDoubleSpinBox;
        s->setRange(-1e9 + 1, 1e9 - 1);
        s->setDecimals(2);
        s->setValue(in.has_default ? in.default_value.to_double() : 0.0);
        s->setGroupSeparatorShown(true);
        extras_[name] = s;
        extraForm_->addRow(QString::fromStdString(in.label.empty() ? in.name : in.label), s);
        connect(s, &QDoubleSpinBox::valueChanged, this, [this]() { recalc(); });
    }
    extraBox_->setVisible(!extras_.isEmpty());
}

void EvaluatePage::recalc() {
    if (!bundle_ || fields_.isEmpty() || !fields_.contains("unpaid")) return;
    auto f = [this](const char* k) { return dec(fields_[k]); };

    TakeHomeInput in;
    in.hourly_wage = f("wage");
    in.hours_per_week = f("hours");
    in.weeks_per_year = f("weeks");
    in.use_statutory_overtime = overtime_->isChecked();
    in.as_of = asOf_->date().toString(Qt::ISODate).toStdString();
    for (auto it = extras_.begin(); it != extras_.end(); ++it) in.extra[it.key().toStdString()] = Value::num(dec(it.value()));

    TakeHomeResult r;
    std::vector<TraceEntry> trace;
    std::string err;
    auto fail = [this](const QString& msg) {
        error_->setText(tr("Cannot calculate for these inputs: %1").arg(msg));
        error_->show();
        for (auto* l : {realHourly_, monthly_, hourlyAfterTax_}) l->setText("–");
        verdict_->clear();
        money_->setRowCount(0);
        time_->setRowCount(0);
        explain_->clear();
    };
    if (!compute_take_home(bundle_, in, r, trace, err)) return fail(QString::fromStdString(err));

    RealHourlyInput rh;
    rh.net_annual = r.net_annual;
    rh.gross_annual = r.gross_annual;
    rh.hours_per_week = in.hours_per_week;
    rh.paid_weeks = in.weeks_per_year;
    rh.paid_vacation_weeks = f("vacation");
    rh.days_per_week = f("days");
    rh.unpaid_minutes_per_day = f("unpaid");
    rh.commute_distance = f("distance");
    rh.commute_minutes = f("minutes");
    rh.cost_per_distance = f("costper");
    rh.daily_parking_and_tolls = f("parking");
    rh.daily_transit_fare = f("transit");
    rh.work_costs_annual = f("workcosts");
    rh.benefits_monthly_value = f("benefits");
    rh.pension_match_percent = f("pension");
    rh.other_benefits_annual = f("otherben");
    RealHourlyResult h;
    if (!compute_real_hourly(rh, h, err)) return fail(QString::fromStdString(err));

    error_->hide();
    const std::string& cur = bundle_->currency;
    auto money = [&](const Decimal& d) { return formatMoney(d, cur); };
    auto num = [](const Decimal& d) { return formatNumber(d); };
    const Decimal twelve = Decimal::from_int(12);
    const Decimal weeks = in.weeks_per_year;

    realHourly_->setText(h.total_hours.is_zero() ? QString("–") : money(h.real_hourly));
    monthly_->setText(money(r.net_monthly));
    hourlyAfterTax_->setText(h.hours_paid.is_zero() ? QString("–") : money(h.take_home_per_hour_paid));
    if (h.total_hours.is_zero() || h.hours_paid.is_zero() || r.net_annual <= Decimal()) {
        verdict_->clear();
    } else {
        const double pct = h.hidden_cost_share.to_double() * 100.0;
        if (pct > 0.5)
            verdict_->setText(tr("Commuting, unpaid time and work costs take about %1% off the after-tax hourly figure.").arg(pct, 0, 'f', 0));
        else if (pct < -0.5)
            verdict_->setText(tr("Benefits and paid vacation lift the real hourly figure about %1% above the after-tax rate.").arg(-pct, 0, 'f', 0));
        else
            verdict_->setText(tr("Extras and costs roughly cancel out."));
    }

    std::vector<Row> rows;
    rows.push_back({tr("Gross pay"), num(r.gross_annual), num(r.gross_annual / twelve), true, 0});
    if (r.overtime_applied)
        rows.push_back({tr("including overtime (%1 h/week at %2)").arg(num(r.overtime_hours_per_week), money(r.overtime_rate)),
                        num(r.overtime_pay_annual), num(r.overtime_pay_annual / twelve), false, 1});
    if (!r.income_tax_lines.empty()) {
        rows.push_back({tr("Income tax"), num(-r.income_tax), num(-r.income_tax / twelve), false, 0});
        for (const auto& l : r.income_tax_lines)
            rows.push_back({QString::fromStdString(l.label), num(-l.amount), num(-l.amount / twelve), false, 1});
    }
    if (!r.contribution_lines.empty()) {
        rows.push_back({tr("Contributions"), num(-r.contributions), num(-r.contributions / twelve), false, 0});
        for (const auto& l : r.contribution_lines)
            rows.push_back({QString::fromStdString(l.label), num(-l.amount), num(-l.amount / twelve), false, 1});
    }
    rows.push_back({tr("Take-home pay"), num(r.net_annual), num(r.net_annual / twelve), true, 0});
    if (!h.benefits_value.is_zero()) rows.push_back({tr("Employer benefits"), num(h.benefits_value), num(h.benefits_value / twelve), false, 0});
    if (!h.commute_cost.is_zero()) rows.push_back({tr("Commuting costs"), num(-h.commute_cost), num(-h.commute_cost / twelve), false, 0});
    if (!h.work_costs.is_zero()) rows.push_back({tr("Work costs"), num(-h.work_costs), num(-h.work_costs / twelve), false, 0});
    rows.push_back({tr("Net value of the job"), num(h.value_annual), num(h.value_annual / twelve), true, 0});
    fillTable(money_, rows);

    auto perWeek = [&](const Decimal& hours) { return weeks.is_zero() ? QString("–") : num(hours / weeks); };
    std::vector<Row> trows;
    trows.push_back({tr("Hours worked"), num(h.hours_worked), perWeek(h.hours_worked), false, 0});
    trows.push_back({tr("Commuting"), num(h.commute_hours), perWeek(h.commute_hours), false, 0});
    trows.push_back({tr("Unpaid time at work"), num(h.unpaid_hours), perWeek(h.unpaid_hours), false, 0});
    trows.push_back({tr("Time the job takes"), num(h.total_hours), perWeek(h.total_hours), true, 0});
    fillTable(time_, trows);

    QString text;
    for (const auto& t : trace) {
        text += QString::fromStdString(t.label.empty() ? t.id : t.label) + "  =  " + QString::fromStdString(t.value.describe()) + "\n";
        text += "    " + QString::fromStdString(t.expr) + "\n";
        QStringList used;
        for (const auto& u : t.used) used << QString::fromStdString(u.first + " = " + u.second);
        if (!used.isEmpty()) text += "    with " + used.join(", ") + "\n";
        text += "    from pack " + QString::fromStdString(t.pack) + "\n\n";
    }
    explain_->setPlainText(text.trimmed());
}
