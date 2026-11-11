#include "JobPickDialog.hpp"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

JobPickDialog::JobPickDialog(const QString& title, const QString& prompt, const QList<Job>& jobs, QWidget* parent) : QDialog(parent) {
    setWindowTitle(title);
    resize(420, 380);
    auto* v = new QVBoxLayout(this);
    v->addWidget(new QLabel(prompt));
    list_ = new QListWidget;
    for (const Job& j : jobs) {
        auto* it = new QListWidgetItem(j.name + "  (" + j.packId + ")");
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(Qt::Checked);
        list_->addItem(it);
    }
    v->addWidget(list_, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto* all = buttons->addButton(tr("Select all"), QDialogButtonBox::ActionRole);
    auto* none = buttons->addButton(tr("Select none"), QDialogButtonBox::ActionRole);
    connect(all, &QPushButton::clicked, this, [this]() { for (int i = 0; i < list_->count(); ++i) list_->item(i)->setCheckState(Qt::Checked); });
    connect(none, &QPushButton::clicked, this, [this]() { for (int i = 0; i < list_->count(); ++i) list_->item(i)->setCheckState(Qt::Unchecked); });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    v->addWidget(buttons);
}

QList<int> JobPickDialog::selected() const {
    QList<int> out;
    for (int i = 0; i < list_->count(); ++i)
        if (list_->item(i)->checkState() == Qt::Checked) out.append(i);
    return out;
}
