#include <QtWidgets/QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    MainWindow w;

    w.uTmaxDir = QDir::homePath();
    w.uTmaxDir.append("/uTmax_files/");
    qDebug() << "Home directory:" << w.uTmaxDir;
    QDir dir(w.uTmaxDir);
    if(!dir.exists())
    {
        qDebug() << "ERROR: Home directory does not exist:" << w.uTmaxDir;
        std::exit(EXIT_FAILURE);
    }

    // Read the default tube data file or ask for the file
    w.dataFileName = w.uTmaxDir;
    w.dataFileName.append("data.csv");
    if (!w.ReadDataFile())
        std::exit(EXIT_FAILURE);

    // Read the calibration file or create a fresh file
    w.calFileName = w.uTmaxDir;
    w.calFileName.append("cal.txt");
    if (!w.ReadCalibration())
        std::exit(EXIT_FAILURE);

    w.SerialPortDiscovery();

    w.show();
    
    return a.exec();
}
