#include "properties_dialog.h"
#include <QVBoxLayout>
#include <QCheckBox>
#include <QPushButton>

PropertiesDialog::PropertiesDialog(QWidget* parent)
    : QDialog(parent)
{
    fontDialog = new QFontDialog(this); // встроенное окно шрифтов
    inspectorModeCheckBox = new QCheckBox("Inspector Mode", this);
    applyButton = new QPushButton("Apply", this);

    auto layout = new QVBoxLayout(this);
    layout->addWidget(fontDialog);
    layout->addWidget(inspectorModeCheckBox);
    layout->addWidget(applyButton);

    connect(applyButton, &QPushButton::clicked, this, &PropertiesDialog::onApplyClicked);
}

bool PropertiesDialog::isInspectorModeEnabled() const {
    return inspectorModeCheckBox->isChecked();
}

void PropertiesDialog::onApplyClicked() {
    emit settingsApplied(); // можно использовать для обновления модели/представления
    accept(); // закрыть окно
}
