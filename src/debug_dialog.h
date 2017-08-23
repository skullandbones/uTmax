#ifndef DEBUG_DIALOG_H
#define DEBUG_DIALOG_H

#include <QDialog>
#include "mainwindow.h"
#include <QTimer>

namespace Ui {
class debug_dialog;
}

class debug_dialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit debug_dialog(MainWindow *parent = 0);
    ~debug_dialog();
    
private slots:
    void UpdateVals();
    void on_OK_clicked();
    void on_Ping_clicked();
    void on_ComSel_currentIndexChanged(const QString &arg1);

    void on_pushPing_clicked();

private:
    Ui::debug_dialog *ui;
    void UpdateComSel();
    MainWindow * mw;
    QTimer * timer;
    bool openPortEn;

};

#endif // DEBUG_DIALOG_H
