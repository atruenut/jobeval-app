#pragma once

#include <QList>
#include <QPointF>
#include <QVariantMap>
#include <QWidget>

#include "JobResult.hpp"

// Custom-painted comparison charts. All views read the same list of results.
class ChartWidget : public QWidget {
    Q_OBJECT
public:
    enum Mode { HourlyBars, MoneyFlow, TimeMoney, Radar, TimeSplit, FitRanking, PayCurve };

    struct Curve {
        QString name;
        QColor color;
        QList<QPointF> points;  // x = hourly-equivalent wage, y = real hourly
        QPointF marker;         // the job's current pay
    };

    explicit ChartWidget(QWidget* parent = nullptr);
    void setMode(Mode m);
    Mode mode() const { return mode_; }
    void setResults(const QList<JobResult>& results, const QVariantMap& priorities);
    void setCurves(const QList<Curve>& curves, const QString& currency);
    QSize minimumSizeHint() const override { return {420, 300}; }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    void drawEmpty(QPainter& p, const QString& text);
    void drawHourlyBars(QPainter& p, const QRectF& r);
    void drawMoneyFlow(QPainter& p, const QRectF& r);
    void drawTimeMoney(QPainter& p, const QRectF& r);
    void drawRadar(QPainter& p, const QRectF& r);
    void drawTimeSplit(QPainter& p, const QRectF& r);
    void drawFitRanking(QPainter& p, const QRectF& r);
    void drawPayCurve(QPainter& p, const QRectF& r);
    void drawLegend(QPainter& p, QRectF& r, const QList<QPair<QString, QColor>>& items);

    Mode mode_ = HourlyBars;
    QList<JobResult> results_;
    QVariantMap priorities_;
    QList<Curve> curves_;
    QString curveCurrency_;
};
