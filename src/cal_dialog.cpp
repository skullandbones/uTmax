#include "cal_dialog.h"
#include "ui_cal_dialog.h"
#include <QDebug>
#define RANGE (0.2)
#define STEPS (2000)
#define RealVal(value) (1+ (float)RANGE*(float)value/(float)(STEPS/2))

Cal_Dialog::Cal_Dialog(MainWindow *parent) :
    QDialog(parent),
    ui(new Ui::Cal_Dialog)
{
    ui->setupUi(this);
    ui->IaSli->setMaximum(STEPS/2);
    ui->IaSli->setMinimum(-STEPS/2);
    ui->IsSli ->setMaximum(STEPS/2);
    ui->IsSli->setMinimum(-STEPS/2);
    ui->VaSli ->setMaximum(STEPS/2);
    ui->VaSli ->setMinimum(-STEPS/2);
    ui->VsSli ->setMaximum(STEPS/2);
    ui->VsSli ->setMinimum(-STEPS/2);
    ui->Vg1Sli ->setMinimum(-STEPS/2);
    ui->Vg1Sli ->setMaximum(STEPS/2);
    ui->Vg4Sli ->setMaximum(STEPS/2);
    ui->Vg4Sli ->setMinimum(-STEPS/2);
    ui->Vg40Sli ->setMaximum(STEPS/2);
    ui->Vg40Sli ->setMinimum(-STEPS/2);
    ui->VsuSli ->setMaximum(STEPS/2);
    ui->VsuSli ->setMinimum(-STEPS/2);
    ui->VnSli ->setMaximum(STEPS/2);
    ui->VnSli ->setMinimum(-STEPS/2);
    mw = parent;
    UpdateDialog(parent);
}

Cal_Dialog::~Cal_Dialog()
{
    delete ui;
}


void Cal_Dialog::on_CancelButton_clicked()
{
    close();
}

void Cal_Dialog::on_SaveButton_clicked()
{
    mw->SaveCalFile();
    close();
}
void Cal_Dialog::UpdateDialog(MainWindow * p)
{
    UpdateSliders(p);
    UpdateLabels(p);
}
void Cal_Dialog::UpdateLabels(MainWindow * p)
{
    ui->VaVal->setText(QString::number(p->calData.VaVal));
    ui->VsVal->setText(QString::number(p->calData.VsVal));
    ui->IaVal->setText(QString::number(p->calData.IaVal));
    ui->IsVal->setText(QString::number(p->calData.IsVal));
    ui->VsuVal->setText(QString::number(p->calData.VsuVal));
    ui->Vg1Val->setText(QString::number(p->calData.Vg1Val));
    ui->Vg4Val->setText(QString::number(p->calData.Vg4Val));
    ui->Vg40Val->setText(QString::number(p->calData.Vg40Val));
    ui->VnVal->setText(QString::number(p->calData.VnVal));
}
void Cal_Dialog::UpdateSliders(MainWindow * p)
{
    ui->VaSli->setValue((int)( STEPS/2 / RANGE * (p->calData.VaVal-1)));
    ui->VsSli->setValue((int)( STEPS/2 / RANGE * (p->calData.VsVal-1)));
    ui->IaSli->setValue((int)( STEPS/2 / RANGE * (p->calData.IaVal-1)));
    ui->IsSli->setValue((int)( STEPS/2 / RANGE * (p->calData.IsVal-1)));
    ui->VsuSli->setValue((int)( STEPS/2 / RANGE * (p->calData.VsuVal-1)));
    ui->Vg1Sli->setValue((int)( STEPS/2 / (RANGE) * (p->calData.Vg1Val-1)));
    ui->Vg4Sli->setValue((int)( STEPS/2 / RANGE * (p->calData.Vg4Val-1)));
    ui->Vg40Sli->setValue((int)( STEPS/2 / RANGE * (p->calData.Vg40Val-1)));
    ui->VnSli->setValue((int)( STEPS/2 / RANGE * (p->calData.VnVal-1)));
}


void Cal_Dialog::on_VaSli_valueChanged(int value)
{
    ui->VaVal->setText(QString::number(RealVal(value)));
    mw->calData.VaVal=RealVal(value);

}

void Cal_Dialog::on_VsSli_valueChanged(int value)
{
    ui->VsVal->setText(QString::number(RealVal(value)));
    mw->calData.VsVal=RealVal(value);
}

void Cal_Dialog::on_IaSli_valueChanged(int value)
{
    ui->IaVal->setText(QString::number(RealVal(value)));
    mw->calData.IaVal=RealVal(value);
}

void Cal_Dialog::on_IsSli_valueChanged(int value)
{
    ui->IsVal->setText(QString::number(RealVal(value)));
    mw->calData.IsVal=RealVal(value);
}

void Cal_Dialog::on_VsuSli_valueChanged(int value)
{
    ui->VsuVal->setText(QString::number(RealVal(value)));
    mw->calData.VsuVal=RealVal(value);
}

void Cal_Dialog::on_Vg1Sli_valueChanged(int value)
{
    ui->Vg1Val->setText(QString::number(RealVal(value)));
    mw->calData.Vg1Val=RealVal(value);
}

void Cal_Dialog::on_Vg4Sli_valueChanged(int value)
{
    ui->Vg4Val->setText(QString::number(RealVal(value)));
    mw->calData.Vg4Val=RealVal(value);
}

void Cal_Dialog::on_Vg40Sli_valueChanged(int value)
{
    ui->Vg40Val->setText(QString::number(RealVal(value)));
    mw->calData.Vg40Val=RealVal(value);
}

void Cal_Dialog::on_VnSli_valueChanged(int value)
{
    ui->VnVal->setText(QString::number(RealVal(value)));
    mw->calData.VnVal=RealVal(value);

}
