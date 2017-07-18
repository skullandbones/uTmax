#ifndef OPTIONS_DIALOG_H
#define OPTIONS_DIALOG_H

#include <QDialog>
#include "mainwindow.h"

namespace Ui {
class options_dialog;
}

class options_dialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit options_dialog(MainWindow *parent = 0);
    ~options_dialog();
    
private slots:
    void on_buttonBox_accepted();
    void on_IaRangeSel_currentIndexChanged(int index);
    void on_IsRangeSel_currentIndexChanged(int index);
    void on_AvgSel_currentIndexChanged(int index);
    void on_IlimitSel_currentIndexChanged(int index);
    void on_Delay_editingFinished();
    void on_AbortOnLimit_clicked(bool checked);

    void on_VgScaleText_returnPressed();

    void on_VgScaleText_editingFinished();

private:
    Ui::options_dialog *ui;
    MainWindow * mw;
    void setIlimit();
    const QList<QString> Ilim400 = QList<QString>()  <<"400" << "350" << "300" << "250" << "200" << "100" << "50" << "24" << "14" << "Off";
    const QList<QString> Ilim200 = QList<QString>()  <<"200" << "175" << "150" << "125" << "100" << "50" << "25" << "12" << "7" << "Off";


};

#endif // OPTIONS_DIALOG_H
