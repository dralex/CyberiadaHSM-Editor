#include "export_file_dialog.h"
#include "ui_export_file_dialog.h"

ExportFileDialog::ExportFileDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ExportFileDialog)
{
    ui->setupUi(this);
}

ExportFileDialog::~ExportFileDialog()
{
    delete ui;
}
