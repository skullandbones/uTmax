#ifndef DATASAVEDIALOG_H
#define DATASAVEDIALOG_H

#include <QDialog>
#include "mainwindow.h"

namespace Ui {
class DataSaveDialog;
}

class DataSaveDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit DataSaveDialog(MainWindow *parent = 0, QString type =0);
    ~DataSaveDialog();
    
private slots:
    void on_buttonBox_accepted();

    void on_buttonBox_rejected();

private:
    Ui::DataSaveDialog *ui;
    MainWindow * mw;
};

#endif // DATASAVEDIALOG_H
