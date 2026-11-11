#pragma once

#include <QDate>
#include <QMap>
#include <QWidget>
#include <memory>

#include "evalcore/catalog.hpp"
#include "evalcore/pack.hpp"

class QDateEdit;
class QDoubleSpinBox;
class QFormLayout;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;

// Step 2: enter a job's pay and hours and see what you actually keep, with every number
// explainable. Recalculates live on every change.
class EvaluatePage : public QWidget {
    Q_OBJECT
public:
    explicit EvaluatePage(QWidget* parent = nullptr);

    void setRegion(const std::shared_ptr<const jobeval::Bundle>& bundle, const jobeval::CatalogEntry* entry,
                   const QString& title);

Q_SIGNALS:
    void changeRegionRequested();
    void inputsChanged();

public:
    // Persisted by the main window.
    double wage() const;
    double hours() const;
    double weeks() const;
    void setInputs(double wage, double hours, double weeks);

private:
    void recalc();
    void buildExtraInputs();

    std::shared_ptr<const jobeval::Bundle> bundle_;
    std::string coverageText_;
    QLabel* regionTitle_;
    QDoubleSpinBox* wage_;
    QDoubleSpinBox* hours_;
    QDoubleSpinBox* weeks_;
    QDateEdit* asOf_;
    QFormLayout* extraForm_;
    QMap<QString, QDoubleSpinBox*> extras_;
    QLabel* monthly_;
    QLabel* yearly_;
    QLabel* hourly_;
    QLabel* error_;
    QTableWidget* table_;
    QLabel* coverage_;
    QPlainTextEdit* explain_;
    QString coverageBase_;
};
