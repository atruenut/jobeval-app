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
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QTableWidget>
#include <QVBoxLayout>

#include "Format.hpp"
#include "JobResult.hpp"
#include "evalcore/notworking.hpp"

using namespace jobeval;

namespace {

void bigNumber(const QString& caption, QLabel*& valueOut, QLabel*& captionOut, QHBoxLayout* row, bool highlight) {
    auto* card = new QWidget;
    card->setObjectName(highlight ? "cardHot" : "card");
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(16, 12, 16, 12);
    v->setSpacing(2);
    auto* cap = new QLabel(caption);
    captionOut = cap;
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

QLabel* note(const QString& text) {
    auto* l = new QLabel(text);
    l->setWordWrap(true);
    l->setObjectName("muted");
    return l;
}

}  // namespace

QString EvaluatePage::suffixFor(Unit u) const {
    const QString c = currency_.isEmpty() ? QString("money") : currency_;
    switch (u) {
        case Unit::Years: return tr(" years");
        case Unit::PercentYear: return tr(" % per year");
        case Unit::Hours: return tr(" h");
        case Unit::Weeks: return tr(" weeks");
        case Unit::Days: return tr(" days");
        case Unit::Minutes: return tr(" min");
        case Unit::Percent: return tr(" %");
        case Unit::Times: return tr(" × wage");
        case Unit::Dist: return " " + unit_->currentText();
        case Unit::PerHour: return " " + c + tr("/hour");
        case Unit::PerDay: return " " + c + tr("/day");
        case Unit::PerMonth: return " " + c + tr("/month");
        case Unit::PerYear: return " " + c + tr("/year");
        case Unit::PerDist: return " " + c + "/" + unit_->currentText();
        case Unit::Money: return " " + c;
        case Unit::Plain: break;
    }
    return QString();
}

EvaluatePage::Field& EvaluatePage::addField(QFormLayout* form, const QString& key, const QString& label, Unit unit, const QString& tip) {
    const FieldSpec* spec = fieldSpec(key);
    auto* s = new QDoubleSpinBox;
    s->setRange(spec->min, spec->max);
    s->setDecimals(spec->decimals);
    s->setValue(spec->def);
    s->setGroupSeparatorShown(true);
    s->setMinimumWidth(120);
    s->setKeyboardTracking(false);
    s->setToolTip(tip);
    auto* l = new QLabel(label);
    l->setToolTip(tip);
    form->addRow(l, s);
    Field f;
    f.box = s;
    f.label = l;
    f.unit = unit;
    connect(s, &QDoubleSpinBox::valueChanged, this, [this]() {
        if (loading_) return;
        recalc();
        emitEdited();
    });
    return fields_[key] = f;
}

EvaluatePage::EvaluatePage(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(20, 14, 20, 14);

    // ---- header: job name, region, notes
    auto* head = new QHBoxLayout;
    name_ = new QLineEdit;
    name_->setObjectName("jobName");
    name_->setPlaceholderText(tr("Job name"));
    name_->setMinimumWidth(360);
    name_->setMaximumWidth(460);
    regionTitle_ = new QLabel;
    regionTitle_->setObjectName("muted");
    auto* change = new QPushButton(tr("Change region…"));
    change->setFlat(true);
    connect(change, &QPushButton::clicked, this, &EvaluatePage::changeRegionRequested);
    kind_ = new QComboBox;
    kind_->addItems({tr("Job"), tr("Not working")});
    kind_->setToolTip(tr("A job, or a not-working situation such as unemployment, studying or living off savings."));
    head->addWidget(name_);
    head->addWidget(kind_);
    head->addWidget(regionTitle_);
    head->addWidget(change);
    head->addStretch(1);
    outer->addLayout(head);

    missing_ = new QLabel;
    missing_->setWordWrap(true);
    missing_->setObjectName("error");
    missing_->hide();
    outer->addWidget(missing_);

    body_ = new QWidget;
    auto* root = new QHBoxLayout(body_);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(20);
    outer->addWidget(body_, 1);

    // ---- left: inputs
    auto* leftInner = new QWidget;
    auto* left = new QVBoxLayout(leftInner);
    left->setContentsMargins(0, 0, 8, 0);

    payBox_ = new QGroupBox(tr("Pay and hours"));
    auto* payV = new QVBoxLayout(payBox_);
    payV->addWidget(note(tr("What the employer pays you, before tax.")));
    payForm_ = new QFormLayout;
    payV->addLayout(payForm_);
    basis_ = new QComboBox;
    basis_->addItems({tr("Hourly wage"), tr("Annual salary")});
    payForm_->addRow(tr("Paid by"), basis_);
    addField(payForm_, "pay", tr("Pay"), Unit::PerHour, tr("Your hourly wage, or your yearly salary, before tax."));
    addField(payForm_, "hours", tr("Hours per week"), Unit::Hours, tr("Paid hours in a normal week, including overtime you usually work."));
    addField(payForm_, "weeks", tr("Paid weeks per year"), Unit::Weeks, tr("52 for a year-round job. Fewer for seasonal or part-year work."));
    addField(payForm_, "days", tr("Days worked per week"), Unit::Days, tr("Used to count commutes and trips."));
    left->addWidget(payBox_);

    otBox_ = new QGroupBox(tr("Overtime"));
    auto* otV = new QVBoxLayout(otBox_);
    otV->addWidget(note(tr("Hours over a weekly limit can pay more. The law sets a minimum; your employer may pay better. "
                           "Company rates never go below the legal minimum.")));
    otForm_ = new QFormLayout;
    otV->addLayout(otForm_);
    otMode_ = new QComboBox;
    otMode_->addItems({tr("No overtime pay"), tr("Legal minimum for this region"), tr("Company policy")});
    otForm_->addRow(tr("Overtime is paid"), otMode_);
    addField(otForm_, "otmult", tr("Company rate"), Unit::Times, tr("Overtime hourly rate as a multiple of your wage: 1.5 for time-and-a-half, 2 for double time."));
    otLegal_ = new QCheckBox(tr("Starts where the law's overtime starts"));
    otForm_->addRow(otLegal_);
    addField(otForm_, "otthr", tr("Company overtime starts after"), Unit::Hours, tr("Weekly hours after which the company overtime rate applies."));
    left->addWidget(otBox_);

    extraPayBox_ = new QGroupBox(tr("Bonuses and extras"));
    auto* extrasV = new QVBoxLayout(extraPayBox_);
    extrasV->addWidget(note(tr("Pay on top of the wage or salary.")));
    auto* extrasForm = new QFormLayout;
    extrasV->addLayout(extrasForm);
    addField(extrasForm, "bonus", tr("Bonus / commission"), Unit::PerYear, tr("Expected yearly bonus, profit share or commission. Taxed as ordinary pay."));
    addField(extrasForm, "signing", tr("Signing bonus"), Unit::Money, tr("One-off payment for joining. Spread over how long you expect to stay."));
    addField(extrasForm, "stay", tr("Expected time in the job"), Unit::Years, tr("Used to spread a signing bonus across the years you expect to stay."));
    addField(extrasForm, "raise", tr("Expected raise per year"), Unit::PercentYear, tr("Where the What if tab in Compare starts its raise assumption. It does not change today's figures."));
    addField(extrasForm, "premium", tr("Shift premium"), Unit::PerHour, tr("Extra pay per hour on evenings, nights, weekends or hazardous work."));
    addField(extrasForm, "premshare", tr("…applies to"), Unit::Percent, tr("Share of your hours the shift premium applies to."));
    left->addWidget(extraPayBox_);

    offBox_ = new QGroupBox(tr("Time off and working from home"));
    auto* offV = new QVBoxLayout(offBox_);
    offV->addWidget(note(tr("Paid time you do not work, and days you work without commuting.")));
    auto* offForm = new QFormLayout;
    offV->addLayout(offForm);
    addField(offForm, "vacation", tr("Paid vacation"), Unit::Weeks, tr("Weeks you are paid but do not work (part of your paid weeks per year). No commute, no hours worked."));
    addField(offForm, "daysoff", tr("Other paid days off"), Unit::Days, tr("Statutory holidays and paid sick days per year that you are paid for but do not work."));
    addField(offForm, "remote", tr("Days worked from home"), Unit::Days, tr("Of your days worked each week, how many are remote. Remote days have no commute, parking or fare."));
    left->addWidget(offBox_);

    goBox_ = new QGroupBox(tr("Getting there"));
    auto* goV = new QVBoxLayout(goBox_);
    goV->addWidget(note(tr("One-way trip to work. Costs are counted for each day you actually work.")));
    auto* go = new QFormLayout;
    goV->addLayout(go);
    unit_ = new QComboBox;
    unit_->addItems({"km", "mi"});
    go->addRow(tr("Distance unit"), unit_);
    addField(go, "distance", tr("Distance one way"), Unit::Dist, QString());
    addField(go, "minutes", tr("Door-to-door time one way"), Unit::Minutes, tr("Include walking, waiting and parking."));
    addField(go, "costper", tr("Vehicle cost per distance"), Unit::PerDist,
             tr("All-in cost of driving one km or mile: fuel, maintenance, tyres, insurance share and depreciation. "
                "Tax authorities publish per-distance rates that make a reasonable guide."));
    addField(go, "parking", tr("Parking and tolls"), Unit::PerDay, tr("Money you pay each day you drive to work."));
    addField(go, "transit", tr("Transit fare"), Unit::PerDay, tr("Total fare for the day's trips (there and back)."));
    left->addWidget(goBox_);

    costBox_ = new QGroupBox(tr("Benefits and costs"));
    auto* costV = new QVBoxLayout(costBox_);
    costV->addWidget(note(tr("Things that make the job worth more or less than the paycheque alone. Enter what each would cost "
                             "you if you had to buy it yourself.")));
    auto* ec = new QFormLayout;
    costV->addLayout(ec);
    addField(ec, "benefits", tr("Employer-paid benefits"), Unit::PerMonth, tr("Value of health, dental, life and disability cover the employer pays for."));
    addField(ec, "premiums", tr("Your benefit premiums and dues"), Unit::PerMonth, tr("Your share of premiums, union dues and similar deductions."));
    addField(ec, "pension", tr("Pension / RRSP match"), Unit::Percent, tr("Employer pension contribution as a percent of your gross pay."));
    addField(ec, "otherben", tr("Other benefits"), Unit::PerYear, tr("Anything else with a cash value: tuition, phone, meals, discounts, stock purchase match."));
    addField(ec, "workcosts", tr("Work costs you pay"), Unit::PerYear, tr("Uniforms, tools, phone, licences and similar."));
    addField(ec, "unpaid", tr("Unpaid time at work"), Unit::Minutes, tr("Per day: unpaid breaks, changing, waiting for shifts. Counted as time the job takes."));
    left->addWidget(costBox_);

    nwBox_ = new QGroupBox(tr("Not working"));
    auto* nwV = new QVBoxLayout(nwBox_);
    nwV->addWidget(note(tr("Compare a job against not working. Money that comes in while you are not working, what living costs, "
                           "and how much of your time the situation uses (job search, classes, caring for someone).")));
    auto* nwForm = new QFormLayout;
    nwV->addLayout(nwForm);
    nwKind_ = new QComboBox;
    nwKind_->addItems({tr("Unemployment"), tr("Student"), tr("Savings / gap time"), tr("Other")});
    nwForm->addRow(tr("Situation"), nwKind_);
    nwEi_ = new QCheckBox(tr("Claim the region's unemployment benefit"));
    nwForm->addRow(nwEi_);
    addField(nwForm, "nw_prior", tr("Earnings before the claim"), Unit::PerYear, tr("What you earned before losing work. The benefit is a share of it, up to a cap set by the region."));
    addField(nwForm, "nw_weeks", tr("Benefit lasts"), Unit::Weeks, tr("How long you expect to be paid. Used for the savings runway and for the tax on year one. Duration depends on your region's unemployment rate and your hours worked; see your government's website. 0 = not limited."));
    addField(nwForm, "nw_taxable", tr("Other taxable income"), Unit::PerMonth, tr("Taxable grants, scholarships, part-time work, interest."));
    addField(nwForm, "nw_taxfree", tr("Tax-free income"), Unit::PerMonth, tr("Child benefits, gifts, loans or lines of credit you draw on, tax-free awards."));
    addField(nwForm, "nw_expenses", tr("Living costs"), Unit::PerMonth, tr("What you spend each month while living this way. Used for how long savings last."));
    addField(nwForm, "nw_savings", tr("Savings available"), Unit::Money, tr("Money you can spend down to cover a shortfall."));
    addField(nwForm, "nw_hours", tr("Time it uses"), Unit::Hours, tr("Hours per week of job search, classes and study, caring or other commitments."));
    nwNote_ = note(QString());
    nwV->addWidget(nwNote_);
    left->addWidget(nwBox_);

    auto* feelBox = new QGroupBox(tr("How it feels"));
    auto* feelV = new QVBoxLayout(feelBox);
    feelV->addWidget(note(tr("Rate what money does not capture, from 0 (bad) to 10 (great). Compare weighs these by how much each matters to you.")));
    auto* feel = new QFormLayout;
    feelV->addLayout(feel);
    for (const Criterion& c : criteria())
        if (!c.computed) addSlider(feel, c.stateKey, c.label, c.help);
    left->addWidget(feelBox);

    extraBox_ = new QGroupBox(tr("Region-specific"));
    extraForm_ = new QFormLayout(extraBox_);
    extraBox_->hide();
    left->addWidget(extraBox_);

    auto* dateBox = new QGroupBox(tr("Rules and notes"));
    auto* dform = new QFormLayout(dateBox);
    asOf_ = new QDateEdit(QDate::currentDate());
    asOf_->setCalendarPopup(true);
    asOf_->setToolTip(tr("Tax and wage rules change by date. Pick the date the job would start paying you."));
    dform->addRow(tr("Rules as of"), asOf_);
    notes_ = new QPlainTextEdit;
    notes_->setPlaceholderText(tr("Notes about this job"));
    notes_->setFixedHeight(70);
    dform->addRow(notes_);
    left->addWidget(dateBox);
    left->addStretch(1);

    auto* scroll = new QScrollArea;
    scroll->setWidget(leftInner);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFixedWidth(450);
    root->addWidget(scroll);

    // ---- right: results
    auto* rightInner = new QWidget;
    auto* right = new QVBoxLayout(rightInner);
    right->setContentsMargins(0, 0, 8, 0);
    auto* cards = new QHBoxLayout;
    bigNumber(tr("Real hourly"), realHourly_, cap1_, cards, true);
    bigNumber(tr("Take-home per month"), monthly_, cap2_, cards, false);
    bigNumber(tr("Per paid hour, after tax"), hourlyAfterTax_, cap3_, cards, false);
    right->addLayout(cards);
    verdict_ = note(QString());
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
    coverage_ = note(QString());
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
    rscroll->setMinimumWidth(500);
    root->addWidget(rscroll, 1);

    for (QComboBox* c : {basis_, otMode_, unit_, nwKind_, kind_}) {
        c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        c->setMinimumContentsLength(12);
    }
    auto changed = [this]() {
        if (loading_) return;
        applyUnits();
        updateVisibility();
        recalc();
        emitEdited();
    };
    connect(kind_, &QComboBox::currentIndexChanged, this, changed);
    connect(nwKind_, &QComboBox::currentIndexChanged, this, changed);
    connect(nwEi_, &QCheckBox::toggled, this, changed);
    connect(basis_, &QComboBox::currentIndexChanged, this, [this, changed]() {
        if (!loading_) convertPayBasis();
        changed();
    });
    connect(otMode_, &QComboBox::currentIndexChanged, this, changed);
    connect(otLegal_, &QCheckBox::toggled, this, changed);
    connect(unit_, &QComboBox::currentIndexChanged, this, changed);
    connect(asOf_, &QDateEdit::dateChanged, this, changed);
    connect(name_, &QLineEdit::textEdited, this, [this]() { emitEdited(); });
    connect(notes_, &QPlainTextEdit::textChanged, this, [this]() { if (!loading_) emitEdited(); });
    applyUnits();
    updateVisibility();
}

void EvaluatePage::addSlider(QFormLayout* form, const QString& key, const QString& label, const QString& tip) {
    auto* row = new QWidget;
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    auto* sl = new QSlider(Qt::Horizontal);
    sl->setRange(0, 10);
    sl->setValue(5);
    sl->setTickPosition(QSlider::TicksBelow);
    sl->setTickInterval(1);
    sl->setToolTip(tip);
    auto* val = new QLabel("5");
    val->setMinimumWidth(18);
    h->addWidget(sl, 1);
    h->addWidget(val);
    auto* l = new QLabel(label);
    l->setToolTip(tip);
    form->addRow(l, row);
    sliders_[key] = sl;
    connect(sl, &QSlider::valueChanged, this, [this, val](int v) {
        val->setText(QString::number(v));
        if (loading_) return;
        emitEdited();
    });
}

// Switching between hourly and salary keeps the pay the same: hourly = salary / (hours x paid
// weeks), salary = hourly x hours x paid weeks.
void EvaluatePage::convertPayBasis() {
    const int now = basis_->currentIndex();
    if (now == lastBasis_) return;
    auto& pay = fields_["pay"];
    const double hours = fields_["hours"].box->value() * fields_["weeks"].box->value();
    if (hours > 0) {
        QSignalBlocker b(pay.box);
        pay.box->setValue(now == 1 ? pay.box->value() * hours : pay.box->value() / hours);
    }
    lastBasis_ = now;
}

void EvaluatePage::applyUnits() {
    for (auto it = fields_.begin(); it != fields_.end(); ++it) it->box->setSuffix(suffixFor(it->unit));
    const bool salary = basis_->currentIndex() == 1;
    fields_["pay"].box->setSuffix(salary ? suffixFor(Unit::PerYear) : suffixFor(Unit::PerHour));
    if (auto* l = qobject_cast<QLabel*>(fields_["pay"].label)) l->setText(salary ? tr("Annual salary") : tr("Hourly wage"));
}

void EvaluatePage::updateVisibility() {
    const bool nw = kind_->currentIndex() == 1;
    payBox_->setVisible(!nw);
    extraPayBox_->setVisible(!nw);
    offBox_->setVisible(!nw);
    goBox_->setVisible(!nw);
    costBox_->setVisible(!nw);
    nwBox_->setVisible(nw);
    const bool unemployment = nwKind_->currentIndex() == 0;
    nwEi_->setVisible(unemployment);
    for (const char* k : {"nw_prior", "nw_weeks"}) {
        fields_[k].box->setVisible(unemployment);
        fields_[k].label->setVisible(unemployment);
    }
    const bool salary = basis_->currentIndex() == 1;
    otBox_->setVisible(!salary && !nw);
    const bool company = otMode_->currentIndex() == 2;
    const bool hasLegal = bundle_ && bundle_->slots.count("overtime_rate") && bundle_->slots.count("overtime_threshold_hours");
    for (const char* k : {"otmult"}) {
        fields_[k].box->setVisible(company);
        fields_[k].label->setVisible(company);
    }
    otLegal_->setVisible(company && hasLegal);
    const bool showThr = company && (!hasLegal || !otLegal_->isChecked());
    fields_["otthr"].box->setVisible(showThr);
    fields_["otthr"].label->setVisible(showThr);
    const bool noLegalNote = !hasLegal;
    otMode_->setItemText(1, noLegalNote ? tr("Legal minimum (this region has none)") : tr("Legal minimum for this region"));
}

QVariantMap EvaluatePage::collectState() const {
    QVariantMap m;
    for (auto it = fields_.begin(); it != fields_.end(); ++it) m[it.key()] = it->box->value();
    for (auto it = sliders_.begin(); it != sliders_.end(); ++it) m[it.key()] = it.value()->value();
    m["kind"] = kind_->currentIndex();
    m["nw_kind"] = nwKind_->currentIndex();
    m["nw_ei"] = nwEi_->isChecked() ? 1 : 0;
    m["basis"] = basis_->currentIndex();
    m["otmode"] = otMode_->currentIndex();
    m["otlegal"] = otLegal_->isChecked() ? 1 : 0;
    m["unit"] = unit_->currentIndex();
    m["asof"] = asOf_->date().toString(Qt::ISODate);
    for (auto it = extras_.begin(); it != extras_.end(); ++it) m["x." + it.key()] = it.value()->value();
    return m;
}

void EvaluatePage::emitEdited() {
    if (loading_) return;
    Q_EMIT jobEdited(name_->text(), notes_->toPlainText(), collectState());
}

void EvaluatePage::setJobName(const QString& name) {
    if (name_->text() != name) name_->setText(name);
}

void EvaluatePage::setJob(const Job& job, const std::shared_ptr<const Bundle>& bundle, const CatalogEntry* entry,
                          const QString& regionTitle) {
    loading_ = true;
    bundle_ = bundle;
    currency_ = bundle ? QString::fromStdString(bundle->currency) : QString();
    const QVariantMap st = normalizedState(job.state);
    name_->setText(job.name);
    notes_->setPlainText(job.notes);
    regionTitle_->setText(regionTitle);
    for (auto it = fields_.begin(); it != fields_.end(); ++it) it->box->setValue(st[it.key()].toDouble());
    for (auto it = sliders_.begin(); it != sliders_.end(); ++it) it.value()->setValue(static_cast<int>(st[it.key()].toDouble()));
    kind_->setCurrentIndex(st["kind"].toInt());
    nwKind_->setCurrentIndex(st["nw_kind"].toInt());
    nwEi_->setChecked(st["nw_ei"].toInt() != 0);
    basis_->setCurrentIndex(st["basis"].toInt());
    lastBasis_ = basis_->currentIndex();
    otMode_->setCurrentIndex(st["otmode"].toInt());
    otLegal_->setChecked(st["otlegal"].toInt() != 0);
    unit_->setCurrentIndex(st["unit"].toInt());
    asOf_->setDate(QDate::fromString(st["asof"].toString(), Qt::ISODate));

    body_->setEnabled(bundle != nullptr);
    missing_->setVisible(bundle == nullptr);
    if (!bundle) {
        missing_->setText(tr("The rules for this job's region (%1) are not downloaded on this computer. "
                             "Use “Change region…” to download them.").arg(job.packId));
        loading_ = false;
        applyUnits();
        updateVisibility();
        error_->hide();
        for (auto* l : {realHourly_, monthly_, hourlyAfterTax_}) l->setText("–");
        verdict_->clear();
        money_->setRowCount(0);
        time_->setRowCount(0);
        coverage_->clear();
        explain_->clear();
        return;
    }

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
    for (auto it = extras_.begin(); it != extras_.end(); ++it) {
        const QString key = "x." + it.key();
        if (st.contains(key)) it.value()->setValue(st[key].toDouble());
    }
    loading_ = false;
    applyUnits();
    updateVisibility();
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
        connect(s, &QDoubleSpinBox::valueChanged, this, [this]() {
            if (loading_) return;
            recalc();
            emitEdited();
        });
    }
    extraBox_->setVisible(!extras_.isEmpty());
}

void EvaluatePage::recalcNotWorking() {
    const NotWorkingInputs in = notWorkingFromState(collectState());
    NotWorkingOutcome o;
    std::vector<TraceEntry> trace;
    std::string err;
    cap1_->setText(tr("Net income per month"));
    cap2_->setText(tr("Savings last"));
    cap3_->setText(tr("Time it uses per week"));
    if (!evaluate_not_working(bundle_, in, o, trace, err)) {
        error_->setText(tr("Cannot calculate for these inputs: %1").arg(QString::fromStdString(err)));
        error_->show();
        for (auto* l : {realHourly_, monthly_, hourlyAfterTax_}) l->setText("–");
        verdict_->clear();
        money_->setRowCount(0);
        time_->setRowCount(0);
        explain_->clear();
        return;
    }
    error_->hide();
    const std::string& cur = bundle_->currency;
    auto money = [&](const Decimal& d) { return formatMoney(d, cur); };
    auto num = [](const Decimal& d) { return formatNumber(d); };
    const Decimal twelve = Decimal::from_int(12);
    const Decimal fiftyTwo = Decimal::from_int(52);

    realHourly_->setText(money(o.net_monthly));
    if (in.monthly_expenses.is_zero())
        monthly_->setText(tr("enter living costs"));
    else if (o.runway_unlimited)
        monthly_->setText(tr("covered"));
    else
        monthly_->setText(tr("%1 months").arg(QString::fromStdString(o.runway_months.to_fixed(1))));
    hourlyAfterTax_->setText(tr("%1 h").arg(QString::fromStdString(in.activity_hours_per_week.to_fixed(1))));

    QString v;
    if (in.use_unemployment_benefit) {
        if (o.pack_has_benefit)
            v = tr("The region's unemployment benefit pays about %1 a week (taxable). %2")
                    .arg(money(o.weekly_benefit), in.benefit_weeks.is_zero() ? tr("Duration depends on your region and hours worked.")
                                                                           : tr("You expect it to last %1 weeks.").arg(QString::fromStdString(in.benefit_weeks.to_fixed(0))));
        else
            v = tr("This region's rules have no unemployment benefit formula yet; enter the amount as other income instead.");
    }
    verdict_->setText(v);

    std::vector<Row> rows;
    if (!o.weekly_benefit.is_zero()) rows.push_back({tr("Unemployment benefit"), num(o.weekly_benefit * fiftyTwo), num(o.weekly_benefit * fiftyTwo / twelve), false, 0});
    if (!in.other_taxable_monthly.is_zero()) rows.push_back({tr("Other taxable income"), num(in.other_taxable_monthly * twelve), num(in.other_taxable_monthly), false, 0});
    rows.push_back({tr("Taxable income"), num(o.taxable_annual), num(o.taxable_annual / twelve), true, 0});
    if (!o.take_home.income_tax_lines.empty()) {
        rows.push_back({tr("Income tax"), num(-o.take_home.income_tax), num(-o.take_home.income_tax / twelve), false, 0});
        for (const auto& l : o.take_home.income_tax_lines)
            rows.push_back({QString::fromStdString(l.label), num(-l.amount), num(-l.amount / twelve), false, 1});
    }
    if (!in.tax_free_monthly.is_zero()) rows.push_back({tr("Tax-free income"), num(in.tax_free_monthly * twelve), num(in.tax_free_monthly), false, 0});
    rows.push_back({tr("Net income"), num(o.net_annual), num(o.net_annual / twelve), true, 0});
    if (!in.monthly_expenses.is_zero()) {
        rows.push_back({tr("Living costs"), num(-in.monthly_expenses * twelve), num(-in.monthly_expenses), false, 0});
        rows.push_back({tr("Left over (or shortfall)"), num(o.net_annual - in.monthly_expenses * twelve), num(o.net_monthly - in.monthly_expenses), true, 0});
    }
    fillTable(money_, rows);
    std::vector<Row> trows;
    trows.push_back({tr("Time this situation uses"), num(o.activity_hours_year), num(in.activity_hours_per_week), true, 0});
    fillTable(time_, trows);
    if (o.benefit_months > Decimal() ) {
        nwNote_->setText(tr("Year-one figures tax only what you receive in twelve months. While the benefit is paid (%1 months) you have about %2 a month; after it ends, %3 a month. Savings last %4 through both stages.")
                             .arg(QString::fromStdString(o.benefit_months.to_fixed(1)), money(o.net_monthly_during), money(o.net_monthly_after),
                                  o.runway_unlimited ? tr("indefinitely") : tr("%1 months").arg(QString::fromStdString(o.runway_months.to_fixed(1)))));
    } else {
        nwNote_->setText(tr("Monthly figures assume this income continues all year (no end date for the benefit entered)."));
    }

    QString text;
    for (const auto& t : trace) {
        text += QString::fromStdString(t.label.empty() ? t.id : t.label) + "  =  " + QString::fromStdString(t.value.describe()) + "\n";
        text += "    " + QString::fromStdString(t.expr) + "\n\n";
    }
    explain_->setPlainText(text.trimmed());
}

void EvaluatePage::recalc() {
    if (!bundle_ || loading_) return;
    if (kind_->currentIndex() == 1) return recalcNotWorking();
    cap1_->setText(tr("Real hourly"));
    cap2_->setText(tr("Take-home per month"));
    cap3_->setText(tr("Per paid hour, after tax"));
    const JobInputs in = inputsFromState(collectState());
    JobOutcome o;
    std::vector<TraceEntry> trace;
    std::string err;
    if (!evaluate_job(bundle_, in, o, trace, err)) {
        error_->setText(tr("Cannot calculate for these inputs: %1").arg(QString::fromStdString(err)));
        error_->show();
        for (auto* l : {realHourly_, monthly_, hourlyAfterTax_}) l->setText("–");
        verdict_->clear();
        money_->setRowCount(0);
        time_->setRowCount(0);
        explain_->clear();
        return;
    }
    error_->hide();
    const TakeHomeResult& r = o.take_home;
    const RealHourlyResult& h = o.real;
    const std::string& cur = bundle_->currency;
    auto money = [&](const Decimal& d) { return formatMoney(d, cur); };
    auto num = [](const Decimal& d) { return formatNumber(d); };
    const Decimal twelve = Decimal::from_int(12);

    realHourly_->setText(h.total_hours.is_zero() ? QString("–") : money(h.real_hourly));
    monthly_->setText(money(r.net_monthly));
    hourlyAfterTax_->setText(h.hours_paid.is_zero() ? QString("–") : money(h.take_home_per_hour_paid));
    if (h.total_hours.is_zero() || h.hours_paid.is_zero() || r.net_annual <= Decimal()) {
        verdict_->clear();
    } else {
        const double pct = h.hidden_cost_share.to_double() * 100.0;
        if (pct > 0.5)
            verdict_->setText(tr("Commuting, unpaid time and costs take about %1% off the after-tax hourly figure.").arg(pct, 0, 'f', 0));
        else if (pct < -0.5)
            verdict_->setText(tr("Benefits and paid time off lift the real hourly figure about %1% above the after-tax rate.").arg(-pct, 0, 'f', 0));
        else
            verdict_->setText(tr("Extras and costs roughly cancel out."));
    }

    std::vector<Row> rows;
    const bool salary = in.basis == PayBasis::Salary;
    rows.push_back({salary ? tr("Salary") : tr("Gross pay"), num(r.gross_annual), num(r.gross_annual / twelve), true, 0});
    if (r.overtime_applied)
        rows.push_back({tr("including overtime (%1 h/week, average %2/h)").arg(num(r.overtime_hours_per_week), money(r.overtime_rate)),
                        num(r.overtime_pay_annual), num(r.overtime_pay_annual / twelve), false, 1});
    if (!r.other_pay_annual.is_zero())
        rows.push_back({tr("including bonus and premiums"), num(r.other_pay_annual), num(r.other_pay_annual / twelve), false, 1});
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
    if (!h.benefit_premiums.is_zero()) rows.push_back({tr("Your premiums and dues"), num(-h.benefit_premiums), num(-h.benefit_premiums / twelve), false, 0});
    rows.push_back({tr("Net value of the job"), num(h.value_annual), num(h.value_annual / twelve), true, 0});
    fillTable(money_, rows);

    const Decimal weeks = in.paid_weeks;
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
