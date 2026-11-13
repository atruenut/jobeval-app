#pragma once

#include <QHBoxLayout>
#include <QList>
#include <QMap>
#include <QVariantMap>
#include <QWidget>

#include "ChartWidget.hpp"
#include "JobModel.hpp"
#include "JobResult.hpp"

class QLabel;
class QComboBox;
class QDoubleSpinBox;
class QSlider;
class QSpinBox;
class QTableWidget;
class QTabWidget;
class QToolButton;

// Jobs side by side: overview table, charts, a weighted "fit" score driven by the person's own
// priorities, and what-if tools (break-even pay, pay curves, pay in N years). Money rankings only
// happen between jobs paid in the same currency, or in currencies the person has given an exchange rate for.
class ComparePage : public QWidget {
    Q_OBJECT
public:
    explicit ComparePage(QWidget* parent = nullptr);
    void refresh(const QList<Job>& jobs, const BundleLookup& lookup, const QMap<QString, QString>& regionNames,
                 const QVariantMap& priorities, const QString& baselineId, const QString& homeCurrency = QString(),
                 const QVariantMap& rates = QVariantMap());

    void showSubTab(int sub, int chart);
    void setScenario(const QVariantMap& values);  // developer aid: pay, raise, years, inflation, hours, commute, remote, bonus, benefits, costs, vacation  // developer/screenshot aid

Q_SIGNALS:
    void prioritiesChanged(const QVariantMap& priorities);
    void baselineChanged(const QString& jobId);
    void currenciesChanged(const QString& home, const QVariantMap& rates);

private:
    void rebuild();          // recompute scores and redraw everything from jobs_/priorities_
    void buildOverview();
    void buildWhatIf();
    void loadScenarioDefaults();
    void onPriorityEdited();
    void rebuildFxBar();     // exchange-rate inputs, shown when the jobs use more than one currency

    QList<Job> jobs_;
    BundleLookup lookup_;
    QVariantMap priorities_;
    QString baselineId_;
    QString home_;           // chosen comparison currency ("" = automatic)
    QVariantMap rates_;      // typed rates: currency -> units of the comparison currency
    QWidget* fxBox_ = nullptr;
    QHBoxLayout* fxLayout_ = nullptr;
    QList<JobResult> results_;
    bool updating_ = false;

    QTabWidget* tabs_;
    // overview
    QLabel* summary_;
    QLabel* warning_;
    QLabel* empty_;
    QComboBox* baseline_;
    QToolButton* rowsButton_;
    QTableWidget* table_;
    QMap<QString, bool> rowVisible_;
    // charts
    QComboBox* chartKind_;
    ChartWidget* chart_;
    QLabel* chartNote_;
    // fit
    QMap<QString, QSlider*> sliders_;
    QMap<QString, QLabel*> sliderValues_;
    ChartWidget* fitChart_;
    QLabel* fitText_;
    // what if
    QComboBox* adjustJob_;
    QComboBox* matchJob_;
    QTabWidget* whatTabs_;
    QDoubleSpinBox *sPay_, *sRaise_, *sYears_, *sInflation_, *sHours_, *sCommute_, *sRemote_, *sBonus_, *sBenefits_, *sCosts_, *sVacation_;
    QDoubleSpinBox* target_;
    QLabel* scenarioText_;
    QTableWidget* scenarioTable_;
    QLabel* breakEven_;
    QTableWidget* sensTable_;
    ChartWidget* curveChart_;
    bool firstLoad_ = true;
};
