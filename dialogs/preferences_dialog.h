#ifndef PREFERENCES_DIALOG_H
#define PREFERENCES_DIALOG_H

#include <QDialog>

class QTableWidget;
class ShortcutMap;
class ShortcutsPage;
class AutoreplacePage;
class TextSchemePanel;

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
    AutoreplacePage* autoreplace = nullptr;
    // the color schemes of the text editors on the Text page, under its title (EDIT-COLOR-3)
    TextSchemePanel* textSchemes = nullptr;
public:
    AutoreplacePage* autoreplacePage() const { return autoreplace; }
    QTableWidget* interpreterTable() const { return interpreters; }
    ShortcutsPage* shortcutsPage() const { return shortcuts; }
    TextSchemePanel* textSchemePanel() const { return textSchemes; }
};

#endif // PREFERENCES_DIALOG_H
