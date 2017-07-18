#include "options_dialog.h"
#include "ui_options_dialog.h"
#include <qmessagebox.h>
#include <QDebug>


options_dialog::options_dialog(MainWindow *parent) :
    QDialog(parent),
    ui(new Ui::options_dialog)
{
    char  buf[16];
    mw = parent;
    ui->setupUi(this);
    //initalize widgets
    ui->AvgSel->setCurrentIndex(mw->options.AvgNum);
    ui->IlimitSel->setCurrentIndex(mw->options.Ilimit);
    sprintf(buf,"%d",mw->options.Delay);
    ui->Delay->setText(buf) ;
    ui->AbortOnLimit->setChecked(mw->options.AbortOnLimit);
    ui->IsRangeSel->setCurrentIndex(mw->options.IsRange);
    ui->VgScaleText->setText(QString::number(mw->options.VgScale));
    setIlimit();
}
void options_dialog::setIlimit() {

    QList<QString> Ilist = Ilim200;

    if (mw->calData.IaMax==400) Ilist = Ilim400;

    for (int i=0; i< ui->IlimitSel->count(); i++ ) {
        ui->IlimitSel->setItemText( i, Ilist.at(i) );
        //qDebug() << "ILim text=" <<  Ilist.at(i);
    }
    if (mw->calData.IsMax==400) Ilist = Ilim400; else Ilist=Ilim200;
    ui->IsLimit->setText( Ilist.at(ui->IlimitSel->currentIndex()) );
    ui->IaRangeSel->setCurrentIndex(mw->options.IaRange);
}

options_dialog::~options_dialog()
{
    delete ui;
}

void options_dialog::on_buttonBox_accepted()
{
    close();
}

void options_dialog::on_IaRangeSel_currentIndexChanged(int index)
{
    mw->options.IaRange= index;
}

void options_dialog::on_IsRangeSel_currentIndexChanged(int index)
{
     mw->options.IsRange= index;
}

void options_dialog::on_AvgSel_currentIndexChanged(int index)
{
    mw->options.AvgNum = index;
}

void options_dialog::on_IlimitSel_currentIndexChanged(int index)
{
    QList<QString> Ilist = Ilim200;
    mw->options.Ilimit= index;
    if (mw->calData.IsMax==400) Ilist = Ilim400;
    ui->IsLimit->setText( Ilist.at(ui->IlimitSel->currentIndex()) );
}

void options_dialog::on_Delay_editingFinished()
{
    bool ok;
    int a = ui->Delay->text().toInt(&ok,10);
    if (ok) mw->options.Delay = a;
    else QMessageBox::information(NULL, "Entry Error", "Invalid entry for delay");
}


void options_dialog::on_AbortOnLimit_clicked(bool checked)
{
    mw->options.AbortOnLimit = checked;
}



void options_dialog::on_VgScaleText_returnPressed()
{
    bool ok;
    float a = ui->VgScaleText->text().toFloat(&ok);
    if (ok) mw->options.VgScale = a;
    else QMessageBox::information(NULL, "Entry Error", "Invalid entry for Vg Scale");

}

void options_dialog::on_VgScaleText_editingFinished()
{
    on_VgScaleText_returnPressed();
}
