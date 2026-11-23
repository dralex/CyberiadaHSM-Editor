#ifndef PREFERENCES_DIALOG_H
#define PREFERENCES_DIALOG_H

#include <QDialog>

class QCheckBox;
class QLabel;
class QLineEdit;
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
    // the line of the git program found for the path typed (EDIT-GIT-1)
    void slotGitProgramChanged();
    void slotGitBrowse();

private:
    void loadFromSettings();
    void saveSettings();

private:
    Ui::PreferencesDialog *ui;
    QColor selectionColor;
    // the plugin interpreters by logical name (EDIT-PLUG-31)
    QTableWidget* interpreters = nullptr;
    // the Git page (EDIT-GIT-1): the program, the line of the program found, Use git
    QLineEdit* gitProgram = nullptr;
    QLabel* gitFound = nullptr;
    QCheckBox* gitEnabled = nullptr;
    ShortcutMap* shortcutMap = nullptr;
    ShortcutsPage* shortcuts = nullptr;
    AutoreplacePage* autoreplace = nullptr;
    // the font and the color schemes of the text editors on the Text page (EDIT-MODE-27, EDIT-COLOR-3)
    TextSchemePanel* textSchemes = nullptr;
public:
    AutoreplacePage* autoreplacePage() const { return autoreplace; }
    QTableWidget* interpreterTable() const { return interpreters; }
    QLineEdit* gitProgramEdit() const { return gitProgram; }
    QLabel* gitFoundLabel() const { return gitFound; }
    QCheckBox* gitEnabledBox() const { return gitEnabled; }
    ShortcutsPage* shortcutsPage() const { return shortcuts; }
    TextSchemePanel* textSchemePanel() const { return textSchemes; }
};

#endif // PREFERENCES_DIALOG_H
