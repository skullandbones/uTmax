#include "datasavedialog.h"
#include "ui_datasavedialog.h"

DataSaveDialog::DataSaveDialog(MainWindow *parent, QString type) :
    QDialog(parent),
    ui(new Ui::DataSaveDialog)
{
    mw = parent;
    ui->setupUi(this);
    ui->Type->setText(type);
}

DataSaveDialog::~DataSaveDialog()
{
    delete ui;
}

void DataSaveDialog::on_buttonBox_accepted()
{
    mw->DataSaveDialog_clicked(ui->Type->text());
}

void DataSaveDialog::on_buttonBox_rejected()
{

}
