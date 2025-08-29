#ifndef PREFERENCES_DIALOG_H
#define PREFERENCES_DIALOG_H

#include <QDialog>

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
};

#endif // PREFERENCES_DIALOG_H
