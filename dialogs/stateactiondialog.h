#ifndef STATEACTIONDIALOG_H
#define STATEACTIONDIALOG_H
#include <QDialog>

class QLineEdit;

class StateActionDialog : public QDialog {
    Q_OBJECT

public:
    StateActionDialog(QWidget* parent = nullptr);

    // QString getTrigger() const;
    // QString getGuard() const;
    QString getBehaviour() const;

private:
    // QLineEdit* triggerEdit;
    // QLineEdit* guardEdit;
    QLineEdit* behaviourEdit;
};

#endif // STATEACTIONDIALOG_H
