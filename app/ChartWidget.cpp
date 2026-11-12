#include "ChartWidget.hpp"

#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <cmath>

#include "Format.hpp"

namespace {

double niceCeil(double v) {
    if (v <= 0) return 1;
    const double e = std::pow(10.0, std::floor(std::log10(v)));
    const double f = v / e;
    const double n = f <= 1 ? 1 : f <= 2 ? 2 : f <= 2.5 ? 2.5 : f <= 5 ? 5 : 10;
    return n * e;
}

QString shortMoney(double v) {
    if (std::abs(v) >= 1000) {
        QString t = QString::number(v / 1000.0, 'f', 2);
        while (t.contains('.') && (t.endsWith('0') || t.endsWith('.'))) t.chop(1);
        return t + "k";
    }
    return QString::number(v, 'f', v == std::floor(v) ? 0 : 2);
}

QString elide(const QString& s, const QFontMetricsF& fm, double w) { return fm.elidedText(s, Qt::ElideRight, w); }

const QList<QColor>& criterionColors() {
    static const QList<QColor> c = {QColor("#1a5fb4"), QColor("#26a269"), QColor("#e5a50a"), QColor("#c01c8c"), QColor("#865e3c"),
                                    QColor("#0b7285"), QColor("#e66100"), QColor("#6f4ab3"), QColor("#a51d2d"), QColor("#3d8b8b"),
                                    QColor("#7a8a1e")};
    return c;
}

}  // namespace

ChartWidget::ChartWidget(QWidget* parent) : QWidget(parent) { setMinimumHeight(300); }

void ChartWidget::setMode(Mode m) {
    mode_ = m;
    update();
}

void ChartWidget::setResults(const QList<JobResult>& results, const QVariantMap& priorities) {
    results_ = results;
    priorities_ = priorities;
    update();
}

void ChartWidget::setCurves(const QList<Curve>& curves, const QString& currency) {
    curves_ = curves;
    curveCurrency_ = currency;
    update();
}

void ChartWidget::drawEmpty(QPainter& p, const QString& text) {
    p.setPen(palette().color(QPalette::PlaceholderText));
    p.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap, text);
}

void ChartWidget::drawLegend(QPainter& p, QRectF& r, const QList<QPair<QString, QColor>>& items) {
    QFontMetricsF fm(p.font());
    double x = r.left();
    double y = r.top();
    const double rowH = fm.height() + 4;
    for (const auto& it : items) {
        const double w = fm.horizontalAdvance(it.first) + 24;
        if (x + w > r.right() && x > r.left()) {
            x = r.left();
            y += rowH;
        }
        p.setPen(Qt::NoPen);
        p.setBrush(it.second);
        p.drawRoundedRect(QRectF(x, y + 3, 10, 10), 2, 2);
        p.setPen(palette().color(QPalette::WindowText));
        p.drawText(QPointF(x + 15, y + fm.ascent() + 1), it.first);
        x += w;
    }
    r.setTop(y + rowH + 6);
}

void ChartWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().color(QPalette::Base));
    QList<JobResult> ok;
    for (const JobResult& r : results_)
        if (r.ok) ok.append(r);
    if (mode_ != PayCurve && ok.isEmpty()) return drawEmpty(p, tr("Nothing to chart yet. Add a job and fill in its details."));
    const QRectF area = QRectF(rect()).adjusted(16, 14, -16, -14);
    switch (mode_) {
        case HourlyBars: drawHourlyBars(p, area); break;
        case MoneyFlow: drawMoneyFlow(p, area); break;
        case TimeMoney: drawTimeMoney(p, area); break;
        case Radar: drawRadar(p, area); break;
        case TimeSplit: drawTimeSplit(p, area); break;
        case FitRanking: drawFitRanking(p, area); break;
        case PayCurve: drawPayCurve(p, area); break;
    }
}

// ---------------------------------------------------------------------------------------------

void ChartWidget::drawHourlyBars(QPainter& p, const QRectF& r0) {
    QList<const JobResult*> jobs;
    for (const JobResult& j : results_)
        if (j.ok && j.hasHourly) jobs.append(&j);
    if (jobs.isEmpty()) return drawEmpty(p, tr("This chart needs at least one job (not a not-working situation)."));
    QRectF r = r0;
    drawLegend(p, r, {{tr("Per paid hour, after tax"), QColor("#9db7d9")}, {tr("Real hourly (after commute, costs, unpaid time, benefits)"), QColor("#1a5fb4")}});
    QFontMetricsF fm(p.font());
    double maxV = 1;
    for (const JobResult* j : jobs) maxV = std::max({maxV, j->realHourly, j->emp.real.take_home_per_hour_paid.to_double()});
    const double top = niceCeil(maxV * 1.08);
    const double labelW = std::min(170.0, r.width() * 0.28);
    QRectF plot(r.left() + labelW, r.top(), r.width() - labelW - 70, r.height() - 22);
    const double rowH = plot.height() / jobs.size();
    p.setPen(QPen(palette().color(QPalette::Mid), 1, Qt::DotLine));
    for (int t = 0; t <= 4; ++t) {
        const double x = plot.left() + plot.width() * t / 4.0;
        p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        p.setPen(palette().color(QPalette::PlaceholderText));
        p.drawText(QPointF(x - 12, plot.bottom() + fm.height()), shortMoney(top * t / 4.0));
        p.setPen(QPen(palette().color(QPalette::Mid), 1, Qt::DotLine));
    }
    for (int i = 0; i < jobs.size(); ++i) {
        const JobResult& j = *jobs[i];
        const double y = plot.top() + i * rowH;
        const double bh = std::min(18.0, rowH * 0.32);
        const double a = j.emp.real.take_home_per_hour_paid.to_double();
        p.setPen(palette().color(QPalette::WindowText));
        p.drawText(QRectF(r.left(), y, labelW - 8, rowH), Qt::AlignVCenter | Qt::AlignRight, elide(j.job.name, fm, labelW - 8));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#9db7d9"));
        p.drawRoundedRect(QRectF(plot.left(), y + rowH / 2 - bh - 1, plot.width() * a / top, bh), 3, 3);
        p.setBrush(j.color.isValid() ? QColor("#1a5fb4") : QColor("#1a5fb4"));
        p.drawRoundedRect(QRectF(plot.left(), y + rowH / 2 + 1, plot.width() * j.realHourly / top, bh), 3, 3);
        p.setPen(palette().color(QPalette::WindowText));
        p.drawText(QPointF(plot.left() + plot.width() * a / top + 6, y + rowH / 2 - 4), QString::number(a, 'f', 2));
        QFont f = p.font();
        f.setBold(true);
        p.setFont(f);
        p.drawText(QPointF(plot.left() + plot.width() * j.realHourly / top + 6, y + rowH / 2 + bh - 2), QString::number(j.realHourly, 'f', 2));
        f.setBold(false);
        p.setFont(f);
    }
}

void ChartWidget::drawMoneyFlow(QPainter& p, const QRectF& r0) {
    QList<const JobResult*> jobs;
    for (const JobResult& j : results_)
        if (j.ok && !j.notWorking) jobs.append(&j);
    if (jobs.isEmpty()) return drawEmpty(p, tr("This chart needs at least one job."));
    QRectF r = r0;
    const QColor cTax("#c64600"), cCon("#e5a50a"), cCost("#a51d2d"), cKeep("#26a269");
    drawLegend(p, r, {{tr("Income tax"), cTax}, {tr("Contributions"), cCon}, {tr("Commute, work costs, premiums"), cCost}, {tr("What you keep (net value)"), cKeep}});
    QFontMetricsF fm(p.font());
    double maxV = 1;
    for (const JobResult* j : jobs) maxV = std::max(maxV, j->emp.take_home.gross_annual.to_double() + j->emp.real.benefits_value.to_double());
    const double top = niceCeil(maxV * 1.05);
    const double labelW = std::min(170.0, r.width() * 0.28);
    QRectF plot(r.left() + labelW, r.top(), r.width() - labelW - 20, r.height() - 22);
    const double rowH = plot.height() / jobs.size();
    for (int t = 0; t <= 4; ++t) {
        const double x = plot.left() + plot.width() * t / 4.0;
        p.setPen(QPen(palette().color(QPalette::Mid), 1, Qt::DotLine));
        p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        p.setPen(palette().color(QPalette::PlaceholderText));
        p.drawText(QPointF(x - 14, plot.bottom() + fm.height()), shortMoney(top * t / 4.0));
    }
    for (int i = 0; i < jobs.size(); ++i) {
        const JobResult& j = *jobs[i];
        const double y = plot.top() + i * rowH;
        const double bh = std::min(30.0, rowH * 0.6);
        p.setPen(palette().color(QPalette::WindowText));
        p.drawText(QRectF(r.left(), y, labelW - 8, rowH), Qt::AlignVCenter | Qt::AlignRight, elide(j.job.name, fm, labelW - 8));
        const double tax = j.emp.take_home.income_tax.to_double();
        const double con = j.emp.take_home.contributions.to_double();
        const double cost = (j.emp.real.commute_cost + j.emp.real.work_costs + j.emp.real.benefit_premiums).to_double();
        const double keep = std::max(0.0, j.emp.real.value_annual.to_double());
        double x = plot.left();
        const double yy = y + (rowH - bh) / 2;
        auto seg = [&](double v, const QColor& c) {
            const double w = plot.width() * v / top;
            if (w <= 0) return;
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            p.drawRect(QRectF(x, yy, w, bh));
            if (w > 44) {
                p.setPen(Qt::white);
                p.drawText(QRectF(x, yy, w, bh), Qt::AlignCenter, shortMoney(v));
            }
            x += w;
        };
        seg(tax, cTax);
        seg(con, cCon);
        seg(cost, cCost);
        seg(keep, cKeep);
    }
}

void ChartWidget::drawTimeMoney(QPainter& p, const QRectF& r0) {
    QRectF r = r0;
    QFontMetricsF fm(p.font());
    QList<QPair<QString, QColor>> legend;
    for (const JobResult& j : results_)
        if (j.ok) legend.append({j.job.name, j.color});
    drawLegend(p, r, legend);
    double maxX = 10, maxY = 1, minY = 0;
    for (const JobResult& j : results_) {
        if (!j.ok) continue;
        maxX = std::max(maxX, j.hoursPerWeek);
        maxY = std::max(maxY, j.monthlyValue);
        minY = std::min(minY, j.monthlyValue);
    }
    const double topX = niceCeil(maxX * 1.1), topY = niceCeil(maxY * 1.1);
    QRectF plot(r.left() + 60, r.top() + 6, r.width() - 80, r.height() - 52);
    auto px = [&](double x) { return plot.left() + plot.width() * x / topX; };
    auto py = [&](double y) { return plot.bottom() - plot.height() * (y - minY) / (topY - minY); };
    for (int t = 0; t <= 4; ++t) {
        const double yv = minY + (topY - minY) * t / 4.0;
        p.setPen(QPen(palette().color(QPalette::Mid), 1, Qt::DotLine));
        p.drawLine(QPointF(plot.left(), py(yv)), QPointF(plot.right(), py(yv)));
        p.setPen(palette().color(QPalette::PlaceholderText));
        p.drawText(QRectF(r.left(), py(yv) - 8, 54, 16), Qt::AlignRight | Qt::AlignVCenter, shortMoney(yv));
        const double xv = topX * t / 4.0;
        p.drawText(QRectF(px(xv) - 24, plot.bottom() + 4, 48, 16), Qt::AlignCenter, QString::number(xv, 'f', 0));
    }
    p.setPen(palette().color(QPalette::PlaceholderText));
    p.drawText(QRectF(plot.left(), plot.bottom() + 22, plot.width(), 18), Qt::AlignCenter, tr("Hours per week the job (or situation) takes  →  more time"));
    p.save();
    p.translate(r.left() + 6, plot.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-plot.height() / 2, -4, plot.height(), 16), Qt::AlignCenter, tr("Net value per month  →  more money"));
    p.restore();
    for (const JobResult& j : results_) {
        if (!j.ok) continue;
        const QPointF c(px(j.hoursPerWeek), py(j.monthlyValue));
        p.setPen(QPen(Qt::white, 2));
        p.setBrush(j.color);
        p.drawEllipse(c, 9, 9);
        p.setPen(palette().color(QPalette::WindowText));
        p.drawText(c + QPointF(13, 4), elide(j.job.name, fm, 150));
    }
}

void ChartWidget::drawRadar(QPainter& p, const QRectF& r0) {
    QRectF r = r0;
    QList<QPair<QString, QColor>> legend;
    for (const JobResult& j : results_)
        if (j.ok) legend.append({j.job.name, j.color});
    drawLegend(p, r, legend);
    const QList<Criterion>& cr = criteria();
    const int n = cr.size();
    const QPointF c = r.center();
    const double rad = std::min(r.width(), r.height()) / 2 - 70;
    if (rad < 40) return;
    QFontMetricsF fm(p.font());
    auto pt = [&](int i, double v) {
        const double a = -M_PI / 2 + 2 * M_PI * i / n;
        return QPointF(c.x() + rad * v / 10.0 * std::cos(a), c.y() + rad * v / 10.0 * std::sin(a));
    };
    p.setBrush(Qt::NoBrush);
    for (int ring = 2; ring <= 10; ring += 2) {
        QPolygonF poly;
        for (int i = 0; i < n; ++i) poly << pt(i, ring);
        p.setPen(QPen(palette().color(QPalette::Mid), 1, ring == 10 ? Qt::SolidLine : Qt::DotLine));
        p.drawPolygon(poly);
    }
    for (int i = 0; i < n; ++i) {
        p.setPen(QPen(palette().color(QPalette::Mid), 1));
        p.drawLine(c, pt(i, 10));
        const QPointF lp = pt(i, 11.6);
        const QString label = cr[i].label;
        const double w = fm.horizontalAdvance(label);
        p.setPen(palette().color(QPalette::WindowText));
        p.drawText(QPointF(lp.x() - (lp.x() < c.x() - 4 ? w : lp.x() > c.x() + 4 ? 0 : w / 2), lp.y() + 4), label);
    }
    for (const JobResult& j : results_) {
        if (!j.ok) continue;
        QPolygonF poly;
        for (int i = 0; i < n; ++i) poly << pt(i, j.scores.value(cr[i].key, 0.0));
        QColor fill = j.color;
        fill.setAlpha(40);
        p.setPen(QPen(j.color, 2));
        p.setBrush(fill);
        p.drawPolygon(poly);
    }
}

void ChartWidget::drawTimeSplit(QPainter& p, const QRectF& r0) {
    QRectF r = r0;
    const QColor cWork("#1a5fb4"), cCom("#e66100"), cUnpaid("#865e3c"), cAct("#26a269");
    drawLegend(p, r, {{tr("Hours worked"), cWork}, {tr("Commuting"), cCom}, {tr("Unpaid time at work"), cUnpaid}, {tr("Not-working commitments"), cAct}});
    QFontMetricsF fm(p.font());
    QList<const JobResult*> jobs;
    double maxV = 1;
    for (const JobResult& j : results_) {
        if (!j.ok) continue;
        jobs.append(&j);
        maxV = std::max(maxV, j.hoursPerWeek);
    }
    const double top = niceCeil(maxV * 1.08);
    const double labelW = std::min(170.0, r.width() * 0.28);
    QRectF plot(r.left() + labelW, r.top(), r.width() - labelW - 30, r.height() - 22);
    const double rowH = plot.height() / jobs.size();
    for (int t = 0; t <= 4; ++t) {
        const double x = plot.left() + plot.width() * t / 4.0;
        p.setPen(QPen(palette().color(QPalette::Mid), 1, Qt::DotLine));
        p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        p.setPen(palette().color(QPalette::PlaceholderText));
        p.drawText(QPointF(x - 10, plot.bottom() + fm.height()), QString::number(top * t / 4.0, 'f', 0) + " h");
    }
    for (int i = 0; i < jobs.size(); ++i) {
        const JobResult& j = *jobs[i];
        const double y = plot.top() + i * rowH;
        const double bh = std::min(30.0, rowH * 0.6);
        p.setPen(palette().color(QPalette::WindowText));
        p.drawText(QRectF(r.left(), y, labelW - 8, rowH), Qt::AlignVCenter | Qt::AlignRight, elide(j.job.name, fm, labelW - 8));
        const double weeks = std::max(1.0, j.job.state.value("weeks").toDouble());
        double work = 0, com = 0, unpaid = 0, act = 0;
        if (j.notWorking) {
            act = j.hoursPerWeek;
        } else {
            work = j.emp.real.hours_worked.to_double() / weeks;
            com = j.emp.real.commute_hours.to_double() / weeks;
            unpaid = j.emp.real.unpaid_hours.to_double() / weeks;
        }
        double x = plot.left();
        const double yy = y + (rowH - bh) / 2;
        auto seg = [&](double v, const QColor& c) {
            const double w = plot.width() * v / top;
            if (w <= 0) return;
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            p.drawRect(QRectF(x, yy, w, bh));
            if (w > 34) {
                p.setPen(Qt::white);
                p.drawText(QRectF(x, yy, w, bh), Qt::AlignCenter, QString::number(v, 'f', 1));
            }
            x += w;
        };
        seg(work, cWork);
        seg(com, cCom);
        seg(unpaid, cUnpaid);
        seg(act, cAct);
    }
}

void ChartWidget::drawFitRanking(QPainter& p, const QRectF& r0) {
    QList<const JobResult*> jobs;
    for (const JobResult& j : results_)
        if (j.ok && j.hasFit) jobs.append(&j);
    if (jobs.isEmpty()) return drawEmpty(p, tr("Give at least one criterion a weight above zero."));
    std::sort(jobs.begin(), jobs.end(), [](const JobResult* a, const JobResult* b) { return a->fit > b->fit; });
    QRectF r = r0;
    const QList<Criterion>& cr = criteria();
    QList<QPair<QString, QColor>> legend;
    for (int i = 0; i < cr.size(); ++i) legend.append({cr[i].label, criterionColors()[i % criterionColors().size()]});
    drawLegend(p, r, legend);
    QFontMetricsF fm(p.font());
    const QVariantMap prio = normalizedPriorities(priorities_);
    double total = 0;
    for (const Criterion& c : cr) total += prio[c.key].toDouble();
    const double labelW = std::min(170.0, r.width() * 0.26);
    QRectF plot(r.left() + labelW, r.top(), r.width() - labelW - 60, r.height());
    const double rowH = plot.height() / jobs.size();
    for (int i = 0; i < jobs.size(); ++i) {
        const JobResult& j = *jobs[i];
        const double y = plot.top() + i * rowH;
        const double bh = std::min(34.0, rowH * 0.62);
        const double yy = y + (rowH - bh) / 2;
        p.setPen(palette().color(QPalette::WindowText));
        QFont f = p.font();
        f.setBold(i == 0);
        p.setFont(f);
        p.drawText(QRectF(r.left(), y, labelW - 8, rowH), Qt::AlignVCenter | Qt::AlignRight, QString("#%1  ").arg(i + 1) + elide(j.job.name, fm, labelW - 40));
        f.setBold(false);
        p.setFont(f);
        double x = plot.left();
        for (int k = 0; k < cr.size(); ++k) {
            if (!j.scores.contains(cr[k].key) || total <= 0) continue;
            // This criterion's share of the 0-100 fit: weight x score / (sum of weights x 10) x 100.
            const double share = prio[cr[k].key].toDouble() * j.scores[cr[k].key] / total * 10.0;
            const double w = plot.width() * share / 100.0;
            p.setPen(Qt::NoPen);
            p.setBrush(criterionColors()[k % criterionColors().size()]);
            p.drawRect(QRectF(x, yy, w, bh));
            x += w;
        }
        p.setPen(QPen(palette().color(QPalette::Mid), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(plot.left(), yy, plot.width(), bh));
        p.setPen(palette().color(QPalette::WindowText));
        f.setBold(true);
        p.setFont(f);
        p.drawText(QPointF(plot.right() + 8, yy + bh / 2 + 5), QString::number(j.fit, 'f', 0));
        f.setBold(false);
        p.setFont(f);
    }
}

void ChartWidget::drawPayCurve(QPainter& p, const QRectF& r0) {
    if (curves_.isEmpty()) return drawEmpty(p, tr("Pay curves need at least one job."));
    QRectF r = r0;
    QList<QPair<QString, QColor>> legend;
    for (const Curve& c : curves_) legend.append({c.name, c.color});
    drawLegend(p, r, legend);
    double minX = 1e18, maxX = -1e18, minY = 1e18, maxY = -1e18;
    for (const Curve& c : curves_)
        for (const QPointF& pt : c.points) {
            minX = std::min(minX, pt.x());
            maxX = std::max(maxX, pt.x());
            minY = std::min(minY, pt.y());
            maxY = std::max(maxY, pt.y());
        }
    if (minX >= maxX) return;
    minY = std::min(0.0, minY);
    const double topY = niceCeil(maxY * 1.05);
    QRectF plot(r.left() + 60, r.top() + 6, r.width() - 80, r.height() - 56);
    auto px = [&](double x) { return plot.left() + plot.width() * (x - minX) / (maxX - minX); };
    auto py = [&](double y) { return plot.bottom() - plot.height() * (y - minY) / (topY - minY); };
    for (int t = 0; t <= 4; ++t) {
        const double yv = minY + (topY - minY) * t / 4.0;
        p.setPen(QPen(palette().color(QPalette::Mid), 1, Qt::DotLine));
        p.drawLine(QPointF(plot.left(), py(yv)), QPointF(plot.right(), py(yv)));
        p.setPen(palette().color(QPalette::PlaceholderText));
        p.drawText(QRectF(r.left(), py(yv) - 8, 54, 16), Qt::AlignRight | Qt::AlignVCenter, shortMoney(yv));
        const double xv = minX + (maxX - minX) * t / 4.0;
        p.drawText(QRectF(px(xv) - 28, plot.bottom() + 4, 56, 16), Qt::AlignCenter, shortMoney(xv));
    }
    p.setPen(palette().color(QPalette::PlaceholderText));
    p.drawText(QRectF(plot.left(), plot.bottom() + 24, plot.width(), 18), Qt::AlignCenter, tr("Hourly wage (%1)  →").arg(curveCurrency_));
    p.save();
    p.translate(r.left() + 6, plot.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-plot.height() / 2, -4, plot.height(), 16), Qt::AlignCenter, tr("Real hourly"));
    p.restore();
    for (const Curve& c : curves_) {
        QPainterPath path;
        for (int i = 0; i < c.points.size(); ++i) {
            const QPointF q(px(c.points[i].x()), py(c.points[i].y()));
            if (i == 0) path.moveTo(q);
            else path.lineTo(q);
        }
        p.setPen(QPen(c.color, 2.2));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
        p.setPen(QPen(Qt::white, 2));
        p.setBrush(c.color);
        p.drawEllipse(QPointF(px(c.marker.x()), py(c.marker.y())), 6, 6);
    }
}
