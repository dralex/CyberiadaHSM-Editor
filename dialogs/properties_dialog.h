#ifndef PROPERTIES_DIALOG_H
#define PROPERTIES_DIALOG_H

#include <QDialog>
#include <QFontDialog>
#include <QCheckBox>
#include <QPushButton>

class PropertiesDialog : public QDialog {
    Q_OBJECT
public:
    explicit PropertiesDialog(QWidget* parent = nullptr);

    bool isInspectorModeEnabled() const;

signals:
    void settingsApplied();

private slots:
    void onApplyClicked();

private:
    QFontDialog* fontDialog;
    QCheckBox* inspectorModeCheckBox;
    QPushButton* applyButton;
};


#endif // PROPERTIES_DIALOG_H
