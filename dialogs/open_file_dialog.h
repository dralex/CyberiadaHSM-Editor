#ifndef OPEN_FILE_DIALOG_H
#define OPEN_FILE_DIALOG_H

#include <QDialog>

namespace Ui {
class OpenFileDialog;
}

class OpenFileDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OpenFileDialog(QWidget *parent = nullptr);
    ~OpenFileDialog();

    QString selectedFile() const;
    bool inspectorModeEnabled() const;
    bool reconstructionEnabled() const;

private slots:
    void slotBrowseButtonClicked();

private:
    Ui::OpenFileDialog *ui;
};

#endif // OPEN_FILE_DIALOG_H
