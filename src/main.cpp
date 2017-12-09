#include <QtWidgets/QApplication>
#include "mainwindow.h"

// Workaround for native qDebug() output not working
void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QByteArray localMsg = msg.toLocal8Bit();

	// Reduce logging output by only using data that contains line numbers
    if (!context.line) return;

    switch (type) {
    case QtDebugMsg:
        std::fprintf(stderr, "Debug: %s (%s:%u)\n", localMsg.constData(), context.file, context.line);
        break;
#if (QT_VERSION >= QT_VERSION_CHECK(5, 5, 0))
    case QtInfoMsg:
        std::fprintf(stderr, "Info: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        break;
#endif
    case QtWarningMsg:
        std::fprintf(stderr, "Warning: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        break;
    case QtCriticalMsg:
        std::fprintf(stderr, "Critical: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        break;
    case QtFatalMsg:
        std::fprintf(stderr, "Fatal: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        std::abort();
    }
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(myMessageOutput);

    QApplication a(argc, argv);

    QFile stylesheet("Style.qss");
    stylesheet.open(QFile::ReadOnly);
    QString setSheet = QLatin1String(stylesheet.readAll());
    a.setStyleSheet(setSheet);

    MainWindow w;

    w.uTmaxDir = QDir::homePath();
    w.uTmaxDir.append("/uTmax_files/");
    qDebug() << "Home directory:" << w.uTmaxDir;
    QDir dir(w.uTmaxDir);
    if(!dir.exists())
    {
        qCritical() << "ERROR: Home directory does not exist:" << w.uTmaxDir;
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
