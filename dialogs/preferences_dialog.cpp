#include "preferences_dialog.h"
#include "ui_preferences_dialog.h"

#include <QColorDialog>
#include <QLocale>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QSpinBox>
#include <QSettings>
#include <QDebug>

#include <QSet>

#include "settings_manager.h"
#include "fontmanager.h"
#include "interface_language.h"

PreferencesDialog::PreferencesDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PreferencesDialog)
{
    ui->setupUi(this);
    // the plugin interpreters: a name of plugin.json, the program it stands for
    QWidget* pluginsTab = new QWidget(this);
    QVBoxLayout* pluginsLayout = new QVBoxLayout(pluginsTab);
    pluginsLayout->addWidget(new QLabel(tr("Plugin interpreters (empty: looked up in PATH)"), pluginsTab));
    interpreters = new QTableWidget(0, 2, pluginsTab);
    interpreters->setObjectName("interpreterTable");
    interpreters->setHorizontalHeaderLabels(QStringList() << tr("Interpreter") << tr("Program"));
    interpreters->horizontalHeader()->setStretchLastSection(true);
    interpreters->verticalHeader()->hide();
    pluginsLayout->addWidget(interpreters);
    ui->tabWidget->addTab(pluginsTab, tr("Plugins"));

    // the interface languages by their own names; the empty code is the system one
    QString system = resolveUiLanguage(QString(), QLocale::system().uiLanguages());
    ui->interfaceLanguageComboBox->addItem(tr("System default (%1)").arg(uiLanguageName(system)), QString());
    for (const UiLanguage& l: availableUiLanguages()) {
        ui->interfaceLanguageComboBox->addItem(l.name, l.code);
    }

    connect(ui->interfaceLanguageComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PreferencesDialog::slotInterfaceLanguageChanged);

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

// the note shows while the choice differs from the language in use
void PreferencesDialog::slotInterfaceLanguageChanged()
{
    QString chosen = resolveUiLanguage(ui->interfaceLanguageComboBox->currentData().toString(),
                                       QLocale::system().uiLanguages());
    ui->interfaceLanguageNote->setVisible(chosen != currentUiLanguage());
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

    int language = ui->interfaceLanguageComboBox->findData(sm.getInterfaceLanguage());
    ui->interfaceLanguageComboBox->setCurrentIndex(language < 0 ? 0 : language);
    slotInterfaceLanguageChanged();
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
    ui->boldActionPrefixCheckBox->setChecked(sm.getBoldActionPrefix());
    ui->boldEventNameCheckBox->setChecked(sm.getBoldEventName());
    ui->boldTransitionEventNameCheckBox->setChecked(sm.getBoldTransitionEventName());
    ui->highlightCodeCheckBox->setChecked(sm.getHighlightCode());
    ui->highlightInExportsCheckBox->setChecked(sm.getHighlightInExports());

    // plugins: the configured interpreters, python3 always offered, a free row last
    QMap<QString, QString> paths = sm.getInterpreterPaths();
    if (!paths.contains("python3")) paths.insert("python3", QString());
    interpreters->setRowCount(0);
    for (QMap<QString, QString>::const_iterator i = paths.begin(); i != paths.end(); i++) {
        int row = interpreters->rowCount();
        interpreters->insertRow(row);
        interpreters->setItem(row, 0, new QTableWidgetItem(i.key()));
        interpreters->setItem(row, 1, new QTableWidgetItem(i.value()));
    }
    interpreters->insertRow(interpreters->rowCount());
}

void PreferencesDialog::saveSettings()
{
    SettingsManager& sm = SettingsManager::instance();

    // general; the language is read at the start only (EDIT-UI-1)
    sm.setInterfaceLanguage(ui->interfaceLanguageComboBox->currentData().toString());
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
    sm.setBoldActionPrefix(ui->boldActionPrefixCheckBox->isChecked());
    sm.setBoldEventName(ui->boldEventNameCheckBox->isChecked());
    sm.setBoldTransitionEventName(ui->boldTransitionEventNameCheckBox->isChecked());
    sm.setHighlightCode(ui->highlightCodeCheckBox->isChecked());
    sm.setHighlightInExports(ui->highlightInExportsCheckBox->isChecked());

    // plugins: a row without a program removes the entry
    QMap<QString, QString> old = sm.getInterpreterPaths();
    QSet<QString> kept;
    for (int row = 0; row < interpreters->rowCount(); row++) {
        QTableWidgetItem* name = interpreters->item(row, 0);
        QTableWidgetItem* path = interpreters->item(row, 1);
        QString n = name ? name->text().trimmed() : QString();
        if (n.isEmpty()) continue;
        QString value = path ? path->text().trimmed() : QString();
        if (!value.isEmpty()) kept.insert(n);
        sm.setInterpreterPath(n, value);
    }
    for (QMap<QString, QString>::const_iterator i = old.begin(); i != old.end(); i++) {
        if (!kept.contains(i.key())) sm.setInterpreterPath(i.key(), QString());
    }
}
