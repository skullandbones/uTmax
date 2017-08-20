#ifndef CAL_DIALOG_H
#define CAL_DIALOG_H

#include <QDialog>
#include "mainwindow.h"

namespace Ui {
class Cal_Dialog;
}

class Cal_Dialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit Cal_Dialog(MainWindow *parent = 0);
    ~Cal_Dialog();

private slots:
    void on_CancelButton_clicked();
    void on_SaveButton_clicked();
    void on_VaSli_valueChanged(int value);
    void on_VsSli_valueChanged(int value);
    void on_IaSli_valueChanged(int value);
    void on_IsSli_valueChanged(int value);
    void on_VsuSli_valueChanged(int value);
    void on_Vg1Sli_valueChanged(int value);
    void on_Vg4Sli_valueChanged(int value);
    void on_Vg40Sli_valueChanged(int value);

    void on_VnSli_valueChanged(int value);

private:
    Ui::Cal_Dialog *ui;
    void UpdateDialog(MainWindow *);
    void UpdateLabels(MainWindow *);
    void UpdateSliders(MainWindow *);
    MainWindow * mw;

};

#endif // CAL_DIALOG_H
