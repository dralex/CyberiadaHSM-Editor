#ifndef PREFERENCES_DIALOG_H
#define PREFERENCES_DIALOG_H

#include <QDialog>

class QTableWidget;
class ShortcutMap;
class ShortcutsPage;

namespace Ui {
class PreferencesDialog;
}

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    // with the keys of a window the dialog has its Shortcuts page (EDIT-KEYS-2)
    explicit PreferencesDialog(QWidget *parent = nullptr, ShortcutMap* keys = nullptr);
    ~PreferencesDialog();

private slots:
    void slotSelectionColorClicked();
    void slotAcceptSettings();
    void slotApplySettings();
    void slotResetSettings();
    void slotInterfaceLanguageChanged();

private:
    void loadFromSettings();
    void saveSettings();

private:
    Ui::PreferencesDialog *ui;
    QColor selectionColor;
    // the plugin interpreters by logical name (EDIT-PLUG-31)
    QTableWidget* interpreters = nullptr;
    ShortcutMap* shortcutMap = nullptr;
    ShortcutsPage* shortcuts = nullptr;
public:
    QTableWidget* interpreterTable() const { return interpreters; }
    ShortcutsPage* shortcutsPage() const { return shortcuts; }
};

#endif // PREFERENCES_DIALOG_H
