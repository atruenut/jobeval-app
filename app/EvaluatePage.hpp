#pragma once

#include <QMap>
#include <QVariantMap>
#include <QWidget>
#include <memory>

#include "evalcore/catalog.hpp"
#include "evalcore/pack.hpp"

class QCheckBox;
class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QFormLayout;
class QGroupBox;
class QLabel;
class QPlainTextEdit;
class QTableWidget;

// Step 2: describe a job and see what you keep and what it really pays per hour of your
// life. Recalculates live on every change.
class EvaluatePage : public QWidget {
    Q_OBJECT
public:
    explicit EvaluatePage(QWidget* parent = nullptr);

    void setRegion(const std::shared_ptr<const jobeval::Bundle>& bundle, const jobeval::CatalogEntry* entry,
                   const QString& title);

    // All user inputs by key, for persistence.
    QVariantMap state() const;
    void setState(const QVariantMap& values);

Q_SIGNALS:
    void changeRegionRequested();
    void inputsChanged();

private:
    void recalc();
    void buildExtraInputs();
    QDoubleSpinBox* addField(QFormLayout* form, const QString& key, const QString& label, double min, double max,
                             int decimals, double value, const QString& suffix, const QString& tip = QString());
    void updateUnitLabels();

    std::shared_ptr<const jobeval::Bundle> bundle_;
    QLabel* regionTitle_;
    QMap<QString, QDoubleSpinBox*> fields_;
    QMap<QString, QDoubleSpinBox*> extras_;
    QCheckBox* overtime_;
    QComboBox* unit_;
    QDateEdit* asOf_;
    QFormLayout* extraForm_;
    QGroupBox* extraBox_;
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
