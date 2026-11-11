#pragma once

#include <QDialog>
#include <QList>

#include "JobModel.hpp"

class QListWidget;

// "Which jobs?" checklist used for importing and exporting.
class JobPickDialog : public QDialog {
    Q_OBJECT
public:
    JobPickDialog(const QString& title, const QString& prompt, const QList<Job>& jobs, QWidget* parent = nullptr);
    QList<int> selected() const;

private:
    QListWidget* list_;
};
