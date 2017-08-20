#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#define TIMER_SET 500
#define ADC_READ_TIMEOUT (30000/TIMER_SET)
#define PING_TIMEOUT (5000/TIMER_SET)
#define RXSUCCESS 1
#define RXTIMEOUT -1
#define RXCONTINUE 0
#define RXINVALID -2
#define HEAT_CNT_MAX 20
#define HEAT_WAIT_SECS 60


#include <QMainWindow>
//#include "../3rdparty/qextserialport-1.2rc/src/qextserialport.h"
//#include "../3rdparty/qextserialport-1.2rc/src/qextserialenumerator.h"
#include <QQueue>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include "typedefs.h"
#include "kb_optimize.h"
#include "dr_optimize.h"
#include <QComboBox>
#include <qcustomplot.h>
#include "plotTabWidget.h"
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>


namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = 0);
    struct calData_t
    {
        float VaVal;
        float VsVal;
        float IaVal;
        float IsVal;
        float VsuVal;
        float Vg1Val;
        float Vg4Val;
        float Vg40Val;
        float VnVal;
        float RaVal;
        float VaMax=400;
        float IaMax=200;
        float IsMax=200;
        float VgMax=-57;
    } ;
    calData_t calData;
    QString dataFileName;
    QString calFileName;
    void ReadCalibration();
    void ReadDataFile();
    ~MainWindow();

    void SaveCalFile();
    void GetReal();
    void DataSaveDialog_clicked(const QString &);

    struct adc_data_t
    {
        int Ia;
        int Ia_raw;
        int Is;
        int Is_raw;
        int Va;
        int Vs;
        int Vsu;
        int Vn;
        int Gain_a;
        int Gain_s;
    };
    // This holds the ADC data from uTracer
    adc_data_t adc_data;

    struct adc_scale_t
    {
        float Ia;
        float Ia_raw;
        float Is;
        float Is_raw;
        float Va;
        float Vs;
        float Vf;
        float Ia400;
        float Ia400_raw;
        float Is400;
        float Is400_raw;
        float Vg;
        float Vsu;
        float Vn;
        float Gain_a;
        float Gain_s;
    };
    adc_scale_t adc_scale;  //the ADC scale factors
    adc_scale_t adc_real;   //the real version of the ADC readings
    struct options_t {
        int IaRange;
        int IsRange;
        int Delay;
        int Ilimit;
        int AvgNum;
        bool AbortOnLimit;
        float VgScale;
    };
    options_t options;

    QByteArray TxString;
    QByteArray statusString;
    QByteArray echoString;
    bool sendADC;
    bool sendPing;
    float VaSteps;
    float VsSteps;
    float VgSteps;
    float VaStart;
    float VsStart;
    float VgStart;
    float VaEnd;
    float VsEnd;
    float VgEnd;
    int VaADC;
    int VsADC;
    int VgADC;
    int VfADC;
    float VaNow;
    float VsNow;
    float VgNow;
    float VfNow;
    //QextSerialPort * portInUse;
    //QList<QextPortInfo> serPortInfo;
    QSerialPort * portInUse;
    QList<QSerialPortInfo> serPortInfo;
    QString comport;
    void OpenComPort(const QString *);


public slots:
    //void onDeviceDiscovered(const QextPortInfo & );
    //void onDeviceRemoved(const QextPortInfo & );
    //TODO Update plot when this happens:
    //void ErrorWeightClicked();


private slots:
    void readData();
    void on_actionExit_triggered();
    void on_actionCal_triggered();
    void on_actionOptions_triggered();
    void on_actionSave_plot_triggered();
    void on_actionSave_Data_triggered();
    void on_actionDebug_triggered();
    void on_TubeSelector_currentIndexChanged(const QString &arg1);
    void on_Start_clicked();
    void on_VsStart_editingFinished();
    void RxData();
    void on_Stop_clicked();
    void on_TubeType_currentIndexChanged(int index);
    void on_actionSave_Spice_Model_triggered();
    //void on_plotDockWidget_dockLocationChanged(const Qt::DockWidgetArea &area);
    //void on_plotDockWidget_topLevelChanged(bool topLevel);
    void on_actionRead_Data_triggered();
    //void on_actionPreferences_triggered();
    void on_AutoPath_editingFinished();
    void on_AddType_clicked();
    void on_DeleteType_clicked();
    void on_checkVsEqVa_clicked();
    void on_checkQuickTest_clicked();
    void on_Tabs_currentChanged(int index);
    void on_Tabs_tabCloseRequested(int index);

    void on_Browse_clicked();

private:
    Ui::MainWindow *ui;
    void PenUpdate();
    void updateLcdsWithModel();
    void SerialPortDiscovery();
    void updateSweepGreying();
    void updateTubeGreying();
    void CreateTestVectors();
    void ClearLCDs();


    enum Status_t { WaitPing, Heating, Heating_wait00, Heating_wait_adc,
                    Sweep_set, Sweep_adc, Idle, wait_adc,
                    hold_ack, hold, heat_done,HeatOff};
    Status_t status;
    QString uTmaxDir;
    int startSweep;
    int VsStep;
    int VgStep;
    int VaStep;
    int curve;
    int heat;
    bool stop;
    bool timer_on;
    int timeout;
    QTimer * timer;
    void saveADCInfo(QByteArray *);
    int GetVa(float);
    int GetVs(float);
    int GetVg(float);
    int GetVf(float);
    bool ok;
    bool newMessage;
    void sendSer();
    QByteArray RxString;
    void StoreData(bool);
    void SetUpPlot();
    bool SetUpSweepParams();
    float Vdi;
    float power;
    void UpdateTitle();

    //test vector store
    struct test_vector_t {
        float Va, Vs, Vg;
    };

    test_vector_t test_vector;
    QList<test_vector_t> *sweepList;

    //results store
    results_t results;
    QList<results_t> *dataStore;
    QList<results_t> *refStore;
    QList<results_t> *activeStore;

    QString QuickRes;

    struct tubeData_t
    {
        QString ID;
        QString Anode;
        QString G2;
        QString G1;
        QString G1b;
        QString Cathode;
        QString G3;
        QString HtrA;
        QString HtrB;
        float HtrVolts;
        QString Model;
        float VaStart;
        float VaEnd;
        int VaStep;
        float VsStart;
        float VsEnd;
        int VsStep;
        float VgStart;
        float VgEnd;
        int VgStep;
        float Vca;
        float Dva;
        float Vcs;
        float Dvs;
        float Vcg;
        float Dvg;
        float powerLim;
        float EmmVa;
        float EmmVs;
        float EmmVg;
        float EmmIa;
    };
    tubeData_t tubeData;
    QList<tubeData_t> *tubeDataList;

    plotInfo_t plot1;

    void LabelPins(tubeData_t);
    void DoPlot(plotInfo_t *  );
    void RePlot(QList<results_t> * );
    bool SaveTubeDataFile();

    QList<QPen> * penList;
    int RxPkt(int len, QByteArray * pCmd, QByteArray * pResponse);
    QFile logFile;
    dr_optimize * optimizer;

    QList<PlotTabWidget*> plotTabs;
    bool ignoreIndexChange;


};
#endif // MAINWINDOW_H
