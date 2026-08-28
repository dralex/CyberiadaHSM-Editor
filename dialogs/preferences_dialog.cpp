#include "preferences_dialog.h"
#include "ui_preferences_dialog.h"

#include <QColorDialog>
#include <QSpinBox>
#include <QSettings>
#include <QDebug>

#include "settings_manager.h"
#include "fontmanager.h"

PreferencesDialog::PreferencesDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PreferencesDialog)
{
    ui->setupUi(this);

    // the forms cannot use the constants, so the limits are set here
    QSpinBox* sizes[] = { ui->stateTitleFontSizeSpinBox, ui->stateActionFontSizeSpinBox,
                          ui->transitionFontSizeSpinBox, ui->commentFontSizeSpinBox };
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        sizes[i]->setRange(FONT_SIZE_MIN, FONT_SIZE_MAX);
    }

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

    ui->printModeCheckBox->setChecked(sm.getPrintMode());
    ui->snapModeCheckBox->setChecked(sm.getSnapMode());

    ui->showTransTextCheckBox->setChecked(sm.getShowTransitionText());
    ui->serviceObjectsCheckBox->setChecked(sm.getShowServiceObjects());

    selectionColor = sm.getSelectionColor();
    ui->selectionColorPreview->setStyleSheet(
        QString("background-color: %1;").arg(selectionColor.name()));
    ui->selectionBorderWidthSpinBox->setValue(sm.getSelectionBorderWidth());
    ui->selectionInvertTextCheckBox->setChecked(sm.getSelectionInvertText());

    ui->gridVisibilityCheckBox->setChecked(sm.getShowGrid());
    ui->gridSpacingSpinBox->setValue(sm.getGridSpacing());

    // the empty setting means the bundled font, so the effective family shows
    QString family = sm.getFontFamily();
    if (family.isEmpty()) {
        family = FontManager::instance().bundledFamily();
    }
    ui->fontFamilyComboBox->setCurrentFont(QFont(family));
    ui->stateTitleFontSizeSpinBox->setValue(sm.getFontSize(fontRoleStateTitle));
    ui->stateActionFontSizeSpinBox->setValue(sm.getFontSize(fontRoleStateAction));
    ui->transitionFontSizeSpinBox->setValue(sm.getFontSize(fontRoleTransition));
    ui->commentFontSizeSpinBox->setValue(sm.getFontSize(fontRoleComment));
}

void PreferencesDialog::saveSettings()
{
    SettingsManager& sm = SettingsManager::instance();

    // general
    sm.setPrintMode(ui->printModeCheckBox->isChecked());
    sm.setSnapMode(ui->snapModeCheckBox->isChecked());

    // visualization
    sm.setShowTransitionText(ui->showTransTextCheckBox->isChecked());
    sm.setShowServiceObjects(ui->serviceObjectsCheckBox->isChecked());

    // selection
    sm.setSelectionColor(selectionColor.name());
    sm.setSelectionBorderWidth(ui->selectionBorderWidthSpinBox->value());
    sm.setSelectionInvertText(ui->selectionInvertTextCheckBox->isChecked());

    // grid
    sm.setShowGrid(ui->gridVisibilityCheckBox->isChecked());
    sm.setGridSpacing(ui->gridSpacingSpinBox->value());

    // text
    sm.setFontFamily(ui->fontFamilyComboBox->currentFont().family());
    sm.setFontSize(fontRoleStateTitle, ui->stateTitleFontSizeSpinBox->value());
    sm.setFontSize(fontRoleStateAction, ui->stateActionFontSizeSpinBox->value());
    sm.setFontSize(fontRoleTransition, ui->transitionFontSizeSpinBox->value());
    sm.setFontSize(fontRoleComment, ui->commentFontSizeSpinBox->value());
}
