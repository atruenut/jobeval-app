#pragma once

#include <QWidget>

#include "evalcore/catalog.hpp"

class QComboBox;
class QLabel;
class QPushButton;
class QProgressBar;
class PackService;

// Step 1: choose where you work. The lists come from the signed pack catalog, so a new
// country or province appears here without any change to the app.
class RegionPage : public QWidget {
    Q_OBJECT
public:
    explicit RegionPage(PackService* service, QWidget* parent = nullptr);

    QString selectedPackId() const;
    void selectPack(const QString& packId);
    // Rebuild the lists from the service's catalog and refresh the status line.
    void reload();
    void setStatus(const QString& text, bool error = false);

Q_SIGNALS:
    void continueRequested(const QString& packId);

private:
    void updateState();
    const jobeval::CatalogEntry* entryFor(const QString& id) const;
    void populateSubregions();

    PackService* service_;
    QComboBox* country_;
    QComboBox* subregion_;
    QLabel* coverage_;
    QLabel* installState_;
    QLabel* message_;
    QPushButton* download_;
    QPushButton* refresh_;
    QPushButton* go_;
    QProgressBar* progress_;
};
