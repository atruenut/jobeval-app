#pragma once

#include <QMainWindow>
#include <QVariantMap>
#include <memory>

#include "evalcore/pack.hpp"

class EvaluatePage;
class PackService;
class QStackedWidget;
class RegionPage;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(PackService* service, const QString& titleSuffix, QWidget* parent = nullptr);

    // Loads a region and shows the evaluator; false (with the reason in the region page) on failure.
    bool openRegion(const QString& packId);
    void showRegionPage();
    void applyState(const QVariantMap& values);  // developer/testing: set input values

private:
    void saveSettings() const;

    PackService* service_;
    QStackedWidget* stack_;
    RegionPage* region_;
    EvaluatePage* evaluate_;
    QString currentPack_;
};
