#include <QtWidgets/QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QFile stylesheet("Style.qss");
    stylesheet.open(QFile::ReadOnly);
    QString setSheet = QLatin1String(stylesheet.readAll());
    a.setStyleSheet(setSheet);

    MainWindow w;
    w.show();
    
    return a.exec();
}
