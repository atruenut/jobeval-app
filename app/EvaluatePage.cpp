#include "EvaluatePage.hpp"

#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "Format.hpp"
#include "evalcore/takehome.hpp"

using namespace jobeval;

namespace {

Decimal dec(const QDoubleSpinBox* sb) {
    return Decimal::parse(QString::number(sb->value(), 'f', sb->decimals()).toStdString());
}

QDoubleSpinBox* spin(double min, double max, int decimals, double value, const QString& suffix = QString()) {
    auto* s = new QDoubleSpinBox;
    s->setRange(min, max);
    s->setDecimals(decimals);
    s->setValue(value);
    s->setSuffix(suffix);
    s->setGroupSeparatorShown(true);
    s->setMinimumWidth(130);
    return s;
}

QLabel* bigNumber(const QString& caption, QLabel*& valueOut, QWidget* parent, QHBoxLayout* row) {
    auto* card = new QWidget(parent);
    card->setObjectName("card");
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(16, 12, 16, 12);
    v->setSpacing(2);
    auto* cap = new QLabel(caption);
    cap->setObjectName("muted");
    valueOut = new QLabel("–");
    valueOut->setObjectName("bigNumber");
    v->addWidget(cap);
    v->addWidget(valueOut);
    row->addWidget(card, 1);
    return valueOut;
}

}  // namespace

EvaluatePage::EvaluatePage(QWidget* parent) : QWidget(parent) {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(24);

    // ---- left: inputs
    auto* left = new QVBoxLayout;
    regionTitle_ = new QLabel;
    regionTitle_->setObjectName("pageTitle");
    regionTitle_->setWordWrap(true);
    auto* change = new QPushButton(tr("Change region"));
    change->setFlat(true);
    connect(change, &QPushButton::clicked, this, &EvaluatePage::changeRegionRequested);
    left->addWidget(regionTitle_);
    left->addWidget(change, 0, Qt::AlignLeft);

    auto* box = new QGroupBox(tr("The job"));
    auto* form = new QFormLayout(box);
    wage_ = spin(0, 9999.99, 2, 20.00);
    hours_ = spin(0, 168, 1, 40.0, tr(" h"));
    weeks_ = spin(0, 52, 0, 52, tr(" wk"));
    asOf_ = new QDateEdit(QDate::currentDate());
    asOf_->setCalendarPopup(true);
    form->addRow(tr("Hourly wage"), wage_);
    form->addRow(tr("Hours per week"), hours_);
    form->addRow(tr("Paid weeks per year"), weeks_);
    form->addRow(tr("Rules as of"), asOf_);
    left->addWidget(box);

    auto* extraBox = new QGroupBox(tr("Region-specific"));
    extraForm_ = new QFormLayout(extraBox);
    extraBox->setObjectName("extraBox");
    left->addWidget(extraBox);
    extraBox->hide();
    left->addStretch(1);
    auto* leftWrap = new QWidget;
    leftWrap->setLayout(left);
    leftWrap->setFixedWidth(340);
    root->addWidget(leftWrap);

    // ---- right: results
    auto* right = new QVBoxLayout;
    auto* cards = new QHBoxLayout;
    bigNumber(tr("Take-home per month"), monthly_, this, cards);
    bigNumber(tr("Take-home per year"), yearly_, this, cards);
    bigNumber(tr("Per hour worked, after tax"), hourly_, this, cards);
    right->addLayout(cards);

    error_ = new QLabel;
    error_->setObjectName("error");
    error_->setWordWrap(true);
    error_->hide();
    right->addWidget(error_);

    table_ = new QTableWidget(0, 3);
    table_->setHorizontalHeaderLabels({tr("Where it goes"), tr("Per year"), tr("Per month")});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->verticalHeader()->hide();
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setShowGrid(false);
    right->addWidget(table_, 1);

    coverage_ = new QLabel;
    coverage_->setWordWrap(true);
    coverage_->setObjectName("muted");
    right->addWidget(coverage_);

    auto* explainBox = new QGroupBox(tr("How this was calculated"));
    explainBox->setCheckable(true);
    explainBox->setChecked(false);
    auto* ev = new QVBoxLayout(explainBox);
    explain_ = new QPlainTextEdit;
    explain_->setReadOnly(true);
    explain_->setMinimumHeight(140);
    explain_->setFont(QFont("monospace"));
    ev->addWidget(explain_);
    explain_->hide();
    connect(explainBox, &QGroupBox::toggled, explain_, &QWidget::setVisible);
    right->addWidget(explainBox);
    root->addLayout(right, 1);

    for (QDoubleSpinBox* s : {wage_, hours_, weeks_}) {
        connect(s, &QDoubleSpinBox::valueChanged, this, [this]() {
            recalc();
            Q_EMIT inputsChanged();
        });
    }
    connect(asOf_, &QDateEdit::dateChanged, this, [this]() { recalc(); });
}

double EvaluatePage::wage() const { return wage_->value(); }
double EvaluatePage::hours() const { return hours_->value(); }
double EvaluatePage::weeks() const { return weeks_->value(); }

void EvaluatePage::setInputs(double wage, double hours, double weeks) {
    for (auto* s : {wage_, hours_, weeks_}) s->blockSignals(true);
    wage_->setValue(wage);
    hours_->setValue(hours);
    weeks_->setValue(weeks);
    for (auto* s : {wage_, hours_, weeks_}) s->blockSignals(false);
    recalc();
}

void EvaluatePage::setRegion(const std::shared_ptr<const Bundle>& bundle, const CatalogEntry* entry, const QString& title) {
    bundle_ = bundle;
    regionTitle_->setText(title);
    QString cov;
    const QString tier = entry ? QString::fromStdString(entry->tier) : QString::fromStdString(bundle->tier);
    cov = tier == "full" ? tr("Full coverage.") : tier == "partial" ? tr("Partial coverage.") : tr("Rough estimate.");
    if (!bundle->not_modelled.empty()) {
        QStringList items;
        for (const auto& n : bundle->not_modelled) {
            QString s = QString::fromStdString(n);
            const int colon = s.indexOf(": ");
            if (colon >= 0) s = s.mid(colon + 2);
            items << s.replace('_', ' ');
        }
        cov += " " + tr("Not included in this estimate: %1.").arg(items.join(", "));
    }
    if (entry && !entry->last_verified.empty()) cov += " " + tr("Rules last verified %1.").arg(QString::fromStdString(entry->last_verified));
    cov += " " + tr("These are estimates for comparing jobs, not payroll-grade figures.");
    coverageBase_ = cov;
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
        auto* s = spin(-1e9 + 1, 1e9 - 1, 2, in.has_default ? in.default_value.to_double() : 0.0);
        extras_[name] = s;
        extraForm_->addRow(QString::fromStdString(in.label.empty() ? in.name : in.label), s);
        connect(s, &QDoubleSpinBox::valueChanged, this, [this]() { recalc(); });
    }
    if (auto* box = findChild<QGroupBox*>("extraBox")) box->setVisible(!extras_.isEmpty());
}

void EvaluatePage::recalc() {
    if (!bundle_) return;
    TakeHomeInput in;
    in.hourly_wage = dec(wage_);
    in.hours_per_week = dec(hours_);
    in.weeks_per_year = dec(weeks_);
    in.as_of = asOf_->date().toString(Qt::ISODate).toStdString();
    for (auto it = extras_.begin(); it != extras_.end(); ++it) in.extra[it.key().toStdString()] = Value::num(dec(it.value()));

    TakeHomeResult r;
    std::vector<TraceEntry> trace;
    std::string err;
    if (!compute_take_home(bundle_, in, r, trace, err)) {
        error_->setText(tr("Cannot calculate for these inputs: %1").arg(QString::fromStdString(err)));
        error_->show();
        monthly_->setText("–");
        yearly_->setText("–");
        hourly_->setText("–");
        table_->setRowCount(0);
        explain_->clear();
        return;
    }
    error_->hide();
    const std::string& cur = bundle_->currency;
    monthly_->setText(formatMoney(r.net_monthly, cur));
    yearly_->setText(formatMoney(r.net_annual, cur));
    hourly_->setText(r.hours_per_year.is_zero() ? QString("–") : formatMoney(r.net_hourly, cur));

    struct Row {
        QString label;
        Decimal annual;
        bool bold;
        int indent;
    };
    std::vector<Row> rows;
    const Decimal twelve = Decimal::from_int(12);
    rows.push_back({tr("Gross pay"), r.gross_annual, true, 0});
    if (!r.income_tax_lines.empty()) {
        rows.push_back({tr("Income tax"), -r.income_tax, false, 0});
        for (const auto& l : r.income_tax_lines) rows.push_back({QString::fromStdString(l.label), -l.amount, false, 1});
    }
    if (!r.contribution_lines.empty()) {
        rows.push_back({tr("Contributions"), -r.contributions, false, 0});
        for (const auto& l : r.contribution_lines) rows.push_back({QString::fromStdString(l.label), -l.amount, false, 1});
    }
    rows.push_back({tr("Take-home pay"), r.net_annual, true, 0});

    table_->setRowCount(static_cast<int>(rows.size()));
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        const Row& row = rows[static_cast<size_t>(i)];
        auto* a = new QTableWidgetItem(QString(row.indent * 4, ' ') + row.label);
        auto* b = new QTableWidgetItem(formatNumber(row.annual));
        auto* c = new QTableWidgetItem(formatNumber(row.annual / twelve));
        b->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        c->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        QFont f = a->font();
        f.setBold(row.bold);
        for (auto* it : {a, b, c}) {
            it->setFont(f);
            if (row.indent > 0) it->setForeground(palette().color(QPalette::PlaceholderText));
        }
        table_->setItem(i, 0, a);
        table_->setItem(i, 1, b);
        table_->setItem(i, 2, c);
    }
    table_->resizeRowsToContents();

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
