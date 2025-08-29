#include "open_file_dialog.h"
#include "ui_open_file_dialog.h"

#include <QFileDialog>


OpenFileDialog::OpenFileDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::OpenFileDialog)
{
    ui->setupUi(this);

    ui->browseButton->setDefault(true);
    QPushButton *okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    if (okButton) {
        okButton->setEnabled(false);
    }
}

OpenFileDialog::~OpenFileDialog()
{
    delete ui;
}

QString OpenFileDialog::selectedFile() const {
    return ui->filePathEdit->text();
}

bool OpenFileDialog::inspectorModeEnabled() const {
    return ui->inspectorCheckBox->isChecked();
}

bool OpenFileDialog::reconstructionEnabled() const {
    return ui->reconstructCheckBox->isChecked();
}

void OpenFileDialog::slotBrowseButtonClicked() {
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open State Machile GraphML file"),
                                                    QDir::currentPath(),
                                                    tr("CyberiadaML graph (*.graphml)"));
    if (!fileName.isEmpty()) {
        ui->filePathEdit->setText(fileName);
        QPushButton *okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
        if (okButton) {
            okButton->setEnabled(true);
            okButton->setDefault(true);
        }
    }
}
