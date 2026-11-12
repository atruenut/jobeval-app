#pragma once

#include <QMap>
#include <QVariantMap>
#include <QWidget>
#include <memory>

#include "JobModel.hpp"
#include "evalcore/catalog.hpp"
#include "evalcore/pack.hpp"

class QCheckBox;
class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QFormLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSlider;
class QTableWidget;

// Edits one job and shows what it pays. Recalculates live on every change.
class EvaluatePage : public QWidget {
    Q_OBJECT
public:
    explicit EvaluatePage(QWidget* parent = nullptr);

    // `bundle` is null when the job's region rules are not downloaded; `regionTitle` names the region.
    void setJob(const Job& job, const std::shared_ptr<const jobeval::Bundle>& bundle,
                const jobeval::CatalogEntry* entry, const QString& regionTitle);
    void setJobName(const QString& name);

Q_SIGNALS:
    void changeRegionRequested();
    // Emitted after any edit; the owner stores these back into the job.
    void jobEdited(const QString& name, const QString& notes, const QVariantMap& state);

private:
    enum class Unit { Plain, Years, PercentYear, Hours, Weeks, Days, Minutes, Percent, Times, Dist, PerHour, PerDay, PerMonth, PerYear, PerDist, Money };
    struct Field {
        QDoubleSpinBox* box = nullptr;
        QWidget* label = nullptr;
        Unit unit = Unit::Plain;
    };

    void recalc();
    void emitEdited();
    void buildExtraInputs();
    Field& addField(QFormLayout* form, const QString& key, const QString& label, Unit unit, const QString& tip);
    void applyUnits();
    void updateVisibility();
    QVariantMap collectState() const;
    void recalcNotWorking();
    void addSlider(QFormLayout* form, const QString& key, const QString& label, const QString& tip);
    void convertPayBasis();
    QString suffixFor(Unit u) const;

    std::shared_ptr<const jobeval::Bundle> bundle_;
    QString currency_;
    bool loading_ = false;
    QLabel* regionTitle_;
    QLineEdit* name_;
    QPlainTextEdit* notes_;
    QLabel* missing_;
    QWidget* body_;
    QMap<QString, Field> fields_;
    QMap<QString, QDoubleSpinBox*> extras_;
    QFormLayout* payForm_;
    QFormLayout* otForm_;
    QGroupBox* otBox_;
    QComboBox* basis_;
    int lastBasis_ = 0;
    QComboBox* kind_;
    QMap<QString, QSlider*> sliders_;
    QGroupBox* payBox_;
    QGroupBox* goBox_;
    QGroupBox* costBox_;
    QGroupBox* nwBox_;
    QComboBox* nwKind_;
    QCheckBox* nwEi_;
    QLabel* nwNote_;
    QComboBox* otMode_;
    QCheckBox* otLegal_;
    QComboBox* unit_;
    QDateEdit* asOf_;
    QFormLayout* extraForm_;
    QGroupBox* extraBox_;
    QLabel* cap1_;
    QLabel* cap2_;
    QLabel* cap3_;
    QLabel* realHourly_;
    QLabel* monthly_;
    QLabel* hourlyAfterTax_;
    QLabel* verdict_;
    QLabel* error_;
    QTableWidget* money_;
    QTableWidget* time_;
    QLabel* coverage_;
    QPlainTextEdit* explain_;
};
