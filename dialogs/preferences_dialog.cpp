#include "preferences_dialog.h"
#include "ui_preferences_dialog.h"

#include <QColorDialog>
#include <QSettings>
#include <QDebug>

#include "settings_manager.h"

PreferencesDialog::PreferencesDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PreferencesDialog)
{
    ui->setupUi(this);

    ui->selectionColorPreview->setFixedSize(30, 30);
    ui->selectionColorPreview->setFrameStyle(QFrame::Panel | QFrame::Sunken);

    connect(ui->selectionColorButton, &QPushButton::clicked,
            this, &PreferencesDialog::slotSelectionColorClicked);
    connect(ui->buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &PreferencesDialog::slotApplySettings);
    connect(ui->buttonBox->button(QDialogButtonBox::Reset), &QPushButton::clicked,
            this, &PreferencesDialog::slotResetSettings);

    loadFromSettings();
}

PreferencesDialog::~PreferencesDialog()
{
    delete ui;
}

void PreferencesDialog::slotSelectionColorClicked()
{
    QColor newColor = QColorDialog::getColor(selectionColor, this,
                                             tr("Select Highlight Color"));
    if (newColor.isValid()) {
        selectionColor = newColor;
        ui->selectionColorPreview->setStyleSheet(
            QString("background-color: %1;").arg(selectionColor.name()));
    }
}

void PreferencesDialog::slotAcceptSettings()
{
    saveSettings();
    accept();
}

void PreferencesDialog::slotApplySettings()
{
    saveSettings();
}

void PreferencesDialog::slotResetSettings()
{
    SettingsManager::instance().loadDefaults();
    loadFromSettings();
}

void PreferencesDialog::loadFromSettings()
{
    SettingsManager& sm = SettingsManager::instance();

    ui->inspectorModeCheckBox->setChecked(sm.getInspectorMode());
    ui->printModeCheckBox->setChecked(sm.getPrintMode());
    ui->snapModeCheckBox->setChecked(sm.getSnapMode());
    ui->polylineModeCheckBox->setChecked(sm.getPolylineMode());

    ui->showTransTextCheckBox->setChecked(sm.getShowTransitionText());

    selectionColor = sm.getSelectionColor();
    ui->selectionColorPreview->setStyleSheet(
        QString("background-color: %1;").arg(selectionColor.name()));
    ui->selectionBorderWidthSpinBox->setValue(sm.getSelectionBorderWidth());
    ui->selectionInvertTextCheckBox->setChecked(sm.getSelectionInvertText());

    ui->gridVisibilityCheckBox->setChecked(sm.getShowGrid());
    ui->gridSpacingSpinBox->setValue(sm.getGridSpacing());
}

void PreferencesDialog::saveSettings()
{
    SettingsManager& sm = SettingsManager::instance();

    // general
    sm.setInspectorMode(ui->inspectorModeCheckBox->isChecked());
    sm.setPrintMode(ui->printModeCheckBox->isChecked());
    sm.setSnapMode(ui->snapModeCheckBox->isChecked());
    sm.setPolylineMode(ui->polylineModeCheckBox->isChecked());

    // visualization
    sm.setShowTransitionText(ui->showTransTextCheckBox->isChecked());

    // selection
    sm.setSelectionColor(selectionColor.name());
    sm.setSelectionBorderWidth(ui->selectionBorderWidthSpinBox->value());
    sm.setSelectionInvertText(ui->selectionInvertTextCheckBox->isChecked());

    // grid
    sm.setShowGrid(ui->gridVisibilityCheckBox->isChecked());
    sm.setGridSpacing(ui->gridSpacingSpinBox->value());
}
