#ifndef EXPORT_FILE_DIALOG_H
#define EXPORT_FILE_DIALOG_H

#include <QDialog>

namespace Ui {
class ExportFileDialog;
}

class ExportFileDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportFileDialog(QWidget *parent = nullptr);
    ~ExportFileDialog();

private:
    Ui::ExportFileDialog *ui;
};

#endif // EXPORT_FILE_DIALOG_H
