#include "stateactiondialog.h"
#include <QVBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QDialogButtonBox>

StateActionDialog::StateActionDialog(QWidget* parent) : QDialog(parent) {
    auto* layout = new QVBoxLayout(this);

    // layout->addWidget(new QLabel("Trigger:"));
    // triggerEdit = new QLineEdit(this);
    // layout->addWidget(triggerEdit);

    // layout->addWidget(new QLabel("Guard:"));
    // guardEdit = new QLineEdit(this);
    // layout->addWidget(guardEdit);

    layout->addWidget(new QLabel("Behaviour:"));
    behaviourEdit = new QLineEdit(this);
    layout->addWidget(behaviourEdit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

// QString StateActionDialog::getTrigger() const {
//     return triggerEdit->text();
// }

// QString StateActionDialog::getGuard() const {
//     return guardEdit->text();
// }

QString StateActionDialog::getBehaviour() const {
    return behaviourEdit->text();
}
