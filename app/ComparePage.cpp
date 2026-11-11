#include "ComparePage.hpp"

#include <QHeaderView>
#include <QLabel>
#include <QMap>
#include <QTableWidget>
#include <QVBoxLayout>

#include "Format.hpp"

using namespace jobeval;

ComparePage::ComparePage(QWidget* parent) : QWidget(parent) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(20, 16, 20, 16);
    auto* title = new QLabel(tr("Compare jobs"));
    title->setObjectName("pageTitle");
    v->addWidget(title);
    summary_ = new QLabel;
    summary_->setWordWrap(true);
    v->addWidget(summary_);
    warning_ = new QLabel;
    warning_->setWordWrap(true);
    warning_->setObjectName("muted");
    v->addWidget(warning_);
    empty_ = new QLabel(tr("Add a job to start. Add two or more to compare them here."));
    empty_->setObjectName("muted");
    v->addWidget(empty_);
    table_ = new QTableWidget;
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setShowGrid(false);
    table_->setAlternatingRowColors(true);
    table_->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->horizontalHeader()->setMinimumSectionSize(140);
    v->addWidget(table_, 1);
}

void ComparePage::refresh(const QList<Job>& jobs, const BundleLookup& lookup, const QMap<QString, QString>& regionNames) {
    struct Col {
        const Job* job = nullptr;
        bool ok = false;
        QString problem;
        QString currency;
        JobOutcome o;
    };
    QList<Col> cols;
    for (const Job& j : jobs) {
        Col c;
        c.job = &j;
        auto bundle = lookup(j.packId);
        if (!bundle) {
            c.problem = tr("rules not downloaded");
        } else {
            c.currency = QString::fromStdString(bundle->currency);
            std::vector<TraceEntry> trace;
            std::string err;
            c.ok = evaluate_job(bundle, inputsFromState(j.state), c.o, trace, err);
            if (!c.ok) c.problem = QString::fromStdString(err);
        }
        cols.append(c);
    }

    empty_->setVisible(jobs.isEmpty());
    table_->setVisible(!jobs.isEmpty());

    // Rank within each currency by real hourly.
    QMap<QString, QList<int>> byCurrency;
    for (int i = 0; i < cols.size(); ++i)
        if (cols[i].ok) byCurrency[cols[i].currency].append(i);
    QVector<int> rank(cols.size(), 0);
    QVector<bool> best(cols.size(), false);
    for (auto it = byCurrency.begin(); it != byCurrency.end(); ++it) {
        QList<int> idx = it.value();
        std::sort(idx.begin(), idx.end(), [&](int a, int b) { return cols[a].o.real.real_hourly > cols[b].o.real.real_hourly; });
        for (int k = 0; k < idx.size(); ++k) rank[idx[k]] = k + 1;
        if (idx.size() > 1) best[idx.first()] = true;
    }

    // Summary sentence.
    QString sum;
    for (auto it = byCurrency.begin(); it != byCurrency.end(); ++it) {
        QList<int> idx = it.value();
        if (idx.size() < 2) continue;
        std::sort(idx.begin(), idx.end(), [&](int a, int b) { return cols[a].o.real.real_hourly > cols[b].o.real.real_hourly; });
        const Col& a = cols[idx[0]];
        const Col& b = cols[idx[1]];
        const QString line = tr("<b>%1</b> pays the most per hour of your time: %2, versus %3 for %4.")
                                 .arg(a.job->name.toHtmlEscaped(), formatMoney(a.o.real.real_hourly, it.key().toStdString()),
                                      formatMoney(b.o.real.real_hourly, it.key().toStdString()), b.job->name.toHtmlEscaped());
        sum += (sum.isEmpty() ? "" : "<br>") + line;
    }
    summary_->setText(sum.isEmpty() && jobs.size() > 0 ? tr("Add another job in the same currency to rank them.") : sum);
    warning_->setText(byCurrency.size() > 1 ? tr("Jobs paid in different currencies are shown side by side but not ranked against each other.") : QString());

    const QStringList rowNames = {tr("Region"), tr("Rank by real hourly"), tr("Real hourly"), tr("Per paid hour, after tax"),
                                  tr("Take-home per month"), tr("Gross pay per year"), tr("Employer benefits per year"),
                                  tr("Commute and work costs per year"), tr("Net value per year"), tr("Hours the job takes, per week")};
    table_->clear();
    table_->setRowCount(rowNames.size());
    table_->setColumnCount(cols.size());
    table_->setVerticalHeaderLabels(rowNames);
    QStringList heads;
    for (const Col& c : cols) heads << c.job->name;
    table_->setHorizontalHeaderLabels(heads);

    for (int ci = 0; ci < cols.size(); ++ci) {
        const Col& c = cols[ci];
        auto put = [&](int row, const QString& text, bool bold = false, bool hot = false) {
            auto* it = new QTableWidgetItem(text);
            it->setTextAlignment(Qt::AlignCenter);
            QFont f = it->font();
            f.setBold(bold);
            it->setFont(f);
            if (hot) it->setBackground(QColor(0xd9, 0xf2, 0xdc));
            table_->setItem(row, ci, it);
        };
        put(0, regionNames.value(c.job->packId, c.job->packId));
        if (!c.ok) {
            for (int r = 1; r < rowNames.size(); ++r) put(r, r == 2 ? c.problem : QStringLiteral("–"));
            continue;
        }
        const std::string cur = c.currency.toStdString();
        const auto& t = c.o.take_home;
        const auto& h = c.o.real;
        const bool top = best[ci];
        put(1, rank[ci] > 0 && byCurrency[c.currency].size() > 1 ? QString("#%1").arg(rank[ci]) : QStringLiteral("–"), true, top);
        put(2, h.total_hours.is_zero() ? QStringLiteral("–") : formatMoney(h.real_hourly, cur), true, top);
        put(3, h.hours_paid.is_zero() ? QStringLiteral("–") : formatMoney(h.take_home_per_hour_paid, cur));
        put(4, formatMoney(t.net_monthly, cur));
        put(5, formatMoney(t.gross_annual, cur));
        put(6, formatMoney(h.benefits_value, cur));
        put(7, formatMoney(h.commute_cost + h.work_costs + h.benefit_premiums, cur));
        put(8, formatMoney(h.value_annual, cur));
        const QString perWeek = jobs[ci].state.value("weeks").toDouble() > 0
                                    ? formatNumber(h.total_hours / Decimal::parse(QString::number(jobs[ci].state.value("weeks").toDouble(), 'f', 0).toStdString()))
                                    : QStringLiteral("–");
        put(9, perWeek);
    }
}
