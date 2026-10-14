#ifndef PREFERENCES_DIALOG_H
#define PREFERENCES_DIALOG_H

#include <QDialog>

class QTableWidget;

namespace Ui {
class PreferencesDialog;
}

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget *parent = nullptr);
    ~PreferencesDialog();

private slots:
    void slotSelectionColorClicked();
    void slotAcceptSettings();
    void slotApplySettings();
    void slotResetSettings();

private:
    void loadFromSettings();
    void saveSettings();

private:
    Ui::PreferencesDialog *ui;
    QColor selectionColor;
    // the plugin interpreters by logical name (EDIT-PLUG-31)
    QTableWidget* interpreters = nullptr;
public:
    QTableWidget* interpreterTable() const { return interpreters; }
};

#endif // PREFERENCES_DIALOG_H
