/* utGui MainWindow Implementation
 *
 * Copyright (C) 2013 Nick Barton http:\\www.bmamps.com
 * The IaCalc() model is derived from the MatLab by (c) Norman Koren.
 * Used here with permission.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "cal_dialog.h"
#include "options_dialog.h"
#include "debug_dialog.h"
#include "datasavedialog.h"
#include <QFile>
#include <QString>
#include <QDebug>
#include <QFileDialog>
#include <QLineEdit>
#include <QtCore/qmath.h>
#include <qmessagebox.h>
#include <qcustomplot.h>
#include <qvector.h>
#include <QColorDialog>
#include <QAction>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>

#define VMIN 2
#define COMDEBUG (false)
#define VGLIM 3 //difference from max
#define Vdi 0.5  //drop across diode D11
#define Vdar 0.75  //drop across darlington
#define DISTIME 40 //time allowed for caps to discharge after an abort
#define min(a,b) (a<b?a:b)


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    if (COMDEBUG) {
        logFile.setFileName("log.txt");
        logFile.open(QIODevice::WriteOnly | QIODevice::Text);
    }

    activeStore=0;
    adc_data.Ia=0;
    adc_data.Ia_raw=0;
    adc_data.Is=0;
    adc_data.Is_raw=0;
    adc_data.Va=0;
    adc_data.Vs=0;
    adc_data.Vsu=0;
    adc_data.Vn=0;
    adc_data.Gain_a=9;
    adc_data.Gain_s=10;
    adc_scale.Ia=    1000.0*5.0/1023.0/18.0;
    adc_scale.Ia_raw=1000.0*5.0/1023.0/18.0;
    adc_scale.Is=    1000.0*5.0/1023.0/18.0;
    adc_scale.Is_raw=1000.0*5.0/1023.0/18.0;
    adc_scale.Ia400=    1000.0*5.0/1023.0/9.0;
    adc_scale.Ia400_raw=1000.0*5.0/1023.0/9.0;
    adc_scale.Is400=    1000.0*5.0/1023.0/9.0;
    adc_scale.Is400_raw=1000.0*5.0/1023.0/9.0;
    adc_scale.Va=(470.0+6.8)/6.8;
    adc_scale.Vs=(470.0+6.8)/6.8;
    adc_scale.Vsu=5.0/1023.0*(1.8+6.8)/1.8;
    adc_scale.Vn=49.0/2.0;
    adc_scale.Gain_a=1.0;
    adc_scale.Gain_s=1.0;
    options.AvgNum=0;
    options.Delay=0;
    options.IaRange=0;
    options.IsRange=0;
    options.Ilimit=0;
    options.AbortOnLimit=true;
    options.VgScale=1.0;
    portInUse = NULL;
    penList = new QList<QPen>;

    timer = NULL;
    status = Idle;
    doStop = false;
    doStart = false;
    sendADC = false;
    sendPing = false;

    VaADC=0;
    VsADC=0;
    VgADC=0;
    VfADC=0;
    VaNow=0;
    VsNow=0;
    VgNow=0;
    VfNow=0;
    dataStore = new QList<results_t>;
    refStore = new QList<results_t>;
    tubeDataList = new QList<tubeData_t>;
    sweepList = new QList<test_vector_t>;
    ui->setupUi(this);
    //once ui is up...
    //qApp->setStyleSheet("QProgressBar::chunk { background-color: #05B8CC;}");
    //Hack for OSX
    //ui->PlotArea->setAutoMargin(false);
    //ui->PlotArea->setMargin(100,100,100,100);
//    ui->plotDockWidget->setMinimumSize(640,485);
    ui->AutoPath->setText(QDir::homePath());
    QPixmap logo = QPixmap(":/uTracer.png");
    ui->Logo->setPixmap(logo);

    ClearLCDs();

    ui->TubeType->addItem(NONE);
    ui->TubeType->addItem(DIODE);
    ui->TubeType->addItem(DUAL_DIODE);
    ui->TubeType->addItem(TRIODE);
    ui->TubeType->addItem(DUAL_TRIODE);
    ui->TubeType->addItem(KOREN_P);
    ui->TubeType->addItem(DERK_P);
    ui->TubeType->addItem(DERK_B);
    ui->TubeType->addItem(DERKE_P);
    ui->TubeType->addItem(DERKE_B);
    ui->checkAutoNumber->setChecked(false);
    on_checkQuickTest_clicked();
    //add the first default plot tab - this will cause currentIdexChanged() to be called
    ignoreIndexChange=false;
    ui->Tabs->setCurrentIndex(1);
    ui->Tabs->setCurrentIndex(0);
    //
    optimizer = new dr_optimize();

    // Initalise the command response structure
    CmdRsp.txState = TxIdle;
    CmdRsp.rxState = RxIdle;
    CmdRsp.timeout = 0;
}

MainWindow::~MainWindow()
{
    qDebug() << "~MainWindow: Destroy everything!";
    delete optimizer;

    // Stop the machine
    StopTheMachine();

    // Close open port
    CloseComPort();

    delete ui;
}

// Serial Port Management
void MainWindow::SerialPortDiscovery()
{
    // Get a list of serial ports
    QList<QSerialPortInfo> serPortInfo = QSerialPortInfo::availablePorts();
    bool found = false;
    qDebug() << "Requested serial port:" << comport;
    foreach (QSerialPortInfo port, serPortInfo) {
        if (port.portName().isEmpty()) continue;
        qDebug() << "Available serial port:" << port.portName();
        if (!found && comport.contains(port.portName())) {
            found = true;
            qDebug() << "Using serial port:" << comport;
            if (!OpenComPort(&comport, false))
            {
                // Failed to open the serial port so set comport to "none"
                comport = "none";
            }
        }
    }
    if (!found) {
        qDebug() << "ERROR: Requested serial port:" << comport << "not found";
        ui->statusBar->showMessage("The requested serial port was not found, try a different one.");
    }
}

bool MainWindow::OpenComPort(const QString *portName, bool updateCalFile)
{
    CloseComPort();

    qDebug() <<"MainWindow::OpenComPort";

    // Create a serial port object
    portInUse = new QSerialPort(this);
    portInUse->setBaudRate(QSerialPort::Baud9600);
    portInUse->setDataBits(QSerialPort::Data8);
    portInUse->setParity(QSerialPort::NoParity );
    portInUse->setStopBits(QSerialPort::OneStop);
    portInUse->setFlowControl(QSerialPort::NoFlowControl);

    // Open requested port
    comport = *portName;
    portInUse->setPortName(comport);
    qDebug() << "MainWindow::OpenComPort" << portInUse->portName();
    QString msg;
    bool openResult;
    if (portInUse->open(QIODevice::ReadWrite))
    {
        openResult = true;
        connect(portInUse, SIGNAL(readyRead()), this, SLOT(readData()));
        msg = QString("Port %1 opened").arg(comport);
        if (updateCalFile)
            SaveCalFile(); //Update cal file with new port info
    }
    else
    {
        delete portInUse;
        portInUse = NULL;
        msg = QString("Port %1 failed to open").arg(comport);
        openResult = false;
    }
    ui->statusBar->showMessage(msg);

    // Probe for the uTracer being present
    if (openResult) RequestOperation((Operation_t) Probe);

    return(openResult);
}

bool MainWindow::CloseComPort()
{
    qDebug() <<"MainWindow::CloseComPort";

    // Close open port
    if (portInUse)
    {
        disconnect(portInUse, SIGNAL(readyRead()), this, SLOT(readData()));
        qDebug() << "Closing Port:" << portInUse->portName();
        portInUse->close();
        delete portInUse;
        portInUse = NULL;
    }
}

void MainWindow::RequestOperation(Operation_t ReqOperation)
{
    switch (ReqOperation)
    {
        case Stop:
        {
            qDebug() << "RequestOperation: Stop";
            doStop = true;
            StartUpMachine();
            break;
        }
        case Probe:
        {
            qDebug() << "RequestOperation: Probe";
            ui->statusBar->showMessage("Starting up...");
            sendADC = true;
            StartUpMachine();
            break;
        }
        case Ping:
        {
            qDebug() << "RequestOperation: Ping";
            sendPing = true;
            StartUpMachine();
            break;
        }
        case ReadADC:
        {
            qDebug() << "RequestOperation: ReadADC";
            sendADC = true;
            StartUpMachine();
            break;
        }
        case Start:
        {
            qDebug() << "RequestOperation: Start";
            doStart = true;
            StartUpMachine();
            break;
        }
        default:
        {
            qDebug() << "ERROR: RequestOperation: Unrecognised operation:" << ReqOperation;
            break;
        }
    }
}

void MainWindow::StartUpMachine()
{
    // Start up the machine
    if (!timer)
    {
        qDebug() << "StartUpMachine: Starting up the machine!";
        timer = new QTimer(this);
        connect(timer, SIGNAL(timeout()), this, SLOT(RxData()));
        timer->start(TIMER_SET);
    }
}

void MainWindow::StopTheMachine()
{
    // Stop the machine
    if (timer)
    {
        qDebug() << "StopTheMachine: Destroying the timer";
        timer->stop();
        disconnect(timer, SIGNAL(timeout()), this, SLOT(RxData()));
        delete timer;
        timer = NULL;
    }
}

void MainWindow::SendCommand(CommandResponse_t *pCmdRsp, bool txLoad, char rxChar)
{
    static CommandResponse_t *pSendCmdRsp = NULL;
    QByteArray txChar;

    if (txLoad)
    {
        pSendCmdRsp = pCmdRsp;
        pSendCmdRsp->txState = TxLoaded;
    }

    switch (pSendCmdRsp->txState)
    {
        case TxLoaded:
        {
            pSendCmdRsp->txPos = 0;
            pSendCmdRsp->rxPos = 0;
            pSendCmdRsp->Response.clear();
            qDebug() << "SendCommand: TX loaded, Cmd len:" << pSendCmdRsp->Command.length() \
                     << "Expected Response len:" << pSendCmdRsp->ExpectedRspLen;

            // Transmit the first character
            txChar.append(pSendCmdRsp->Command.at(pSendCmdRsp->txPos));
            qDebug() << "SendCommand: send char:" << txChar;
            if (portInUse) portInUse->write(txChar);
            pSendCmdRsp->txState = TxSending;
            pSendCmdRsp->rxState = RxIdle;
            break;
        }
        case TxSending:
        {
            // Check TX was echoed OK
            if (rxChar == pSendCmdRsp->Command.at(pSendCmdRsp->txPos))
            {
                pSendCmdRsp->rxState = RxEchoed;
                qDebug() << "SendCommand: RX Echo:" << rxChar;
            }
            else
            {
                pSendCmdRsp->rxState = RxEchoError;
                qDebug() << "ERROR: SendCommand: Echo failed:" << rxChar;
                pSendCmdRsp->txState = TxIdle;
                break;
            }

            pSendCmdRsp->txPos++;

            if (pSendCmdRsp->txPos < pSendCmdRsp->Command.length())
            {
                // Transmit the next character
                txChar.append(pSendCmdRsp->Command.at(pSendCmdRsp->txPos));
                qDebug() << "SendCommand: send char:" << txChar;
                if (portInUse) portInUse->write(txChar);
            }
            else
            {
                if (pSendCmdRsp->rxPos == pSendCmdRsp->ExpectedRspLen)
                {
                    // No response message was expected
                    qDebug() << "SendCommand: RX response: None";
                    pSendCmdRsp->rxState = RxComplete;
                    pSendCmdRsp->txState = TxComplete;
                }
                else
                {
                    // Now receiving the RX response message
                    qDebug() << "SendCommand: RX response: Wait for message";
                    pSendCmdRsp->rxState = RxResponse;
                    pSendCmdRsp->txState = TxRxing;
                }
            }
            break;
        }
        case TxRxing:
        {
            if (pSendCmdRsp->rxPos < pSendCmdRsp->ExpectedRspLen)
            {
                // Receiving the RX response message
                qDebug() << "SendCommand: RX response:" << rxChar;
                pSendCmdRsp->Response.append(rxChar);
                pSendCmdRsp->rxState = RxResponse;
                pSendCmdRsp->rxPos++;
            }
            if (pSendCmdRsp->rxPos == pSendCmdRsp->ExpectedRspLen)
            {
                // Received the complete RX response message
                qDebug() << "SendCommand: RX response: Complete";
                pSendCmdRsp->rxState = RxComplete;
                pSendCmdRsp->txState = TxComplete;
            }
            break;
        }
        default:
        {
            qDebug() << "ERROR: SendCommand: Unknown TX state:" << pSendCmdRsp->txState;
        }
    }
}

void MainWindow::readData()
{
    char rxChar;

    if (portInUse)
    {
        while (portInUse->bytesAvailable())
        {
            rxChar = portInUse->read(1).at(0);
            SendCommand(NULL, false, rxChar);
        }
    }
}

void MainWindow::RxPkt(CommandResponse_t *pSendCmdRsp, QByteArray *response, RxStatus_t *pRxStatus)
{
    *pRxStatus = RXCONTINUE;

    if (pSendCmdRsp->timeout)
    {
        pSendCmdRsp->timeout--;

        if (pSendCmdRsp->timeout == 0)
        {
            qDebug() << "RxPkt: TIMEOUT - try reading for missing RX";
            readData();

            *pRxStatus = RXTIMEOUT;
        }
    }
    else
    {
        qDebug() << "RxPkt: RXIDLE";
        *pRxStatus = RXIDLE;
    }

    if (pSendCmdRsp->rxState == RxComplete)
    {
        if (pSendCmdRsp->Response.length())
        {
            qDebug() << "RxPkt: Processing RX response:" << pSendCmdRsp->Response;
            *response = pSendCmdRsp->Response;
        }

        pSendCmdRsp->txState = TxIdle;
        pSendCmdRsp->rxState = RxIdle;

        pSendCmdRsp->timeout = 0;

        // For the debug window
        TxString = pSendCmdRsp->Command;
        echoString = pSendCmdRsp->Command;
        statusString = pSendCmdRsp->Response;

        *pRxStatus = RXSUCCESS;
    }

    if (*pRxStatus == RXTIMEOUT)
    {
        // Failed to recover, so abandon the command / response
        qDebug() << "RxPkt: TIMEOUT - failed to recover";
        response->clear();
        pSendCmdRsp->txState = TxIdle;
        pSendCmdRsp->rxState = RxIdle;
    }
}

void MainWindow::SendStartMeasurementCommand(CommandResponse_t *pSendCmdRsp, uint8_t limits, uint8_t averaging,
                                             uint8_t screenGain, uint8_t anodeGain)
{
    char temp_buffer[30];

    qDebug() << "Send Start Measurement command: limits:" << limits << "averaging:" << averaging \
             << "Screen Gain:" <<  screenGain << "Anode Gain:" << anodeGain;

    std::snprintf(temp_buffer, 19, "0000000000%02X%02X%02X%02X", limits, averaging, screenGain, anodeGain);
    pSendCmdRsp->Command = temp_buffer;

    pSendCmdRsp->ExpectedRspLen = 0;
    pSendCmdRsp->timeout = PING_TIMEOUT;
    SendCommand(pSendCmdRsp, true, 0);
}

void MainWindow::SendGetMeasurementCommand(CommandResponse_t *pSendCmdRsp, uint16_t anodeV, uint16_t screenV,
                                           uint16_t gridV, uint16_t filamentV)
{
    char temp_buffer[30];

    qDebug() << "Send Get Measurement command: anodeV:" << anodeV << "screenV" << screenV \
             << "gridV" << gridV << "filamentV:" << filamentV;

    std::snprintf(temp_buffer, 19, "10%04X%04X%04X%04X", anodeV, screenV, gridV, filamentV);
    pSendCmdRsp->Command = temp_buffer;

    pSendCmdRsp->ExpectedRspLen = 38;
    pSendCmdRsp->timeout = ADC_READ_TIMEOUT;
    SendCommand(pSendCmdRsp, true, 0);
}

void MainWindow::SendHoldMeasurementCommand(CommandResponse_t *pSendCmdRsp, uint16_t anodeV, uint16_t screenV,
                                           uint16_t gridV, uint16_t filamentV, int delay)
{
    char temp_buffer[30];

    qDebug() << "Send Hold Measurement command: anodeV:" << anodeV << "screenV" << screenV \
             << "gridV" << gridV << "filamentV:" << filamentV << "delay" << delay;

    std::snprintf(temp_buffer, 19, "20%04X%04X%04X%04X", anodeV, screenV, gridV, filamentV);
    pSendCmdRsp->Command = temp_buffer;

    pSendCmdRsp->ExpectedRspLen = 0;
    pSendCmdRsp->timeout = ADC_READ_TIMEOUT + delay;
    SendCommand(pSendCmdRsp, true, 0);
}

void MainWindow::SendEndMeasurementCommand(CommandResponse_t *pSendCmdRsp)
{
    qDebug() << "Send End Measurement command:";
    pSendCmdRsp->Command = "300000000000000000";
    pSendCmdRsp->ExpectedRspLen = 0;
    pSendCmdRsp->timeout = PING_TIMEOUT;
    SendCommand(pSendCmdRsp, true, 0);
}

void MainWindow::SendFilamentCommand(CommandResponse_t *pSendCmdRsp, uint16_t filamentV)
{
    char temp_buffer[30];

    qDebug() << "Send Filament command: Filament:" << filamentV;

    std::snprintf(temp_buffer, 19, "40000000000000%04X", filamentV);
    pSendCmdRsp->Command = temp_buffer;

    pSendCmdRsp->ExpectedRspLen = 0;
    pSendCmdRsp->timeout = PING_TIMEOUT;
    SendCommand(pSendCmdRsp, true, 0);
}

void MainWindow::SendADCCommand(CommandResponse_t *pSendCmdRsp)
{
    qDebug() << "Send ADC command:";
    pSendCmdRsp->Command = "500000000000000000";
    pSendCmdRsp->ExpectedRspLen = 38;
    pSendCmdRsp->timeout = PING_TIMEOUT;
    SendCommand(pSendCmdRsp, true, 0);
}

//---------------------------------------------
// The main control process
void MainWindow::RxData()
{
    static int time;
    static int delay;
    static int HV_Discharge_Timer;
    //I Limits         200,  175,  150,  125, 100,    50,   25,   12,  7mA,  Off
    static int lim[]={0x8f, 0x8d, 0xad, 0xab, 0x84, 0xa4, 0xa2, 0xa1, 0x80, 0x00};
    static int avg[]={0x40, 1, 2, 4, 8, 16, 32, 0x40};
    static int Ir[]={8,1,2,3,4,5,6,7};
    static QByteArray response;
    static int distime;
    RxStatus_t RxCode;
    static bool repeatTest = false;
    static interrupted_t interrupted = {false, Idle, ""};

    // ---------------------------------------------------
    // Sanity check that the port is still OK
    if (!portInUse)
    {
        ui->statusBar->showMessage("ERROR: The serial port is not available");
        StopTheMachine();
        return;
    }

    if (portInUse && !portInUse->isOpen())
    {
        ui->statusBar->showMessage("ERROR: The serial port closed unexpectedly!");
        StopTheMachine();
        return;
    }

    // ---------------------------------------------------
    // Check for the uTracer response
    RxPkt(&CmdRsp, &response, &RxCode);
    // Check for timeout
    if (RxCode == RXTIMEOUT)
    {
        qDebug() << "RxData: Action RxCode RXTIMEOUT";
        ui->statusBar->showMessage("No response from uTracer. Check cables and power cycle");
        status = Idle;
        StopTheMachine();
        return;
    }

    // Check for invalid response
    if (RxCode == RXINVALID)
    {
        qDebug() << "RxData: Action RxCode RXINVALID";
        ui->statusBar->showMessage("Unexpected response from uTracer; power cycle and restart");
        status = Idle;
        StopTheMachine();
        return;
    }

    // ---------------------------------------------------
    // Deal with user requests, communications must be idle
    if ((RxCode == RXIDLE || RxCode == RXSUCCESS) && !interrupted.cmd)
    {
        if (doStop == true)
        {
            qDebug() << "RxData: Action doStop";
            doStop = false;

            switch (status)
            {
                case Idle:
                case Heating:
                case heat_done:
                {
                    doStop = false;
                    qDebug() << "RxData: Stop heating";
                    HV_Discharge_Timer = 0;
                    ui->HeaterProg->setValue(0);
                    ui->statusBar->showMessage("Heater off");
                    SendFilamentCommand(&CmdRsp, 0);
                    status = wait_stop;
                    break;
                }
                case Sweep_set:
                case Sweep_adc:
                case hold:
                {
                    doStop = false;
                    qDebug() << "RxData: Stop sweeping";
                    float v = VaNow > VsNow ? VaNow : VsNow;
                    distime = (int)(DISTIME * v / 400);
                    HV_Discharge_Timer = distime;
                    ui->statusBar->showMessage("Abort:Heater off");
                    SendFilamentCommand(&CmdRsp, 0);
                    ui->CaptureProg->setValue(0);
                    status = wait_stop;
                    break;
                }
                default:
                {
                    qDebug() << "ERROR: RxData: Cannot stop from state:" << status_name[status];
                    break;
                }
            }
        }
        else if (sendPing == true)
        {
            qDebug() << "RxData: Action sendPing";
            interrupted.cmd = true;
            interrupted.status = status;
            interrupted.response = response;
            status = send_ping;
            sendPing = false;
        }
        else if (sendADC == true)
        {
            qDebug() << "RxData: Action sendADC";
            interrupted.cmd = true;
            interrupted.status = status;
            interrupted.response = response;
            status = read_adc;
            sendADC = false;
        }
        else if (doStart == true)
        {
            qDebug() << "RxData: Action doStart";
            doStart = false;
            if (!SetUpSweepParams()) return;

            switch (status)
            {
                case Idle:
                {
                    qDebug() << "RxData: Start from Idle";
                    repeatTest = false;
                    status = start_sweep_heater;
                    break;
                }
                case Heating:
                {
                    qDebug() << "RxData: Jump to max heating";
                    heat = HEAT_CNT_MAX;
                    break;
                }
                case heat_done:
                {
                    qDebug() << "RxData: Start the sweep test now";
                    if (dataStore->length() > 0) dataStore->clear();
                    CreateTestVectors();
                    curve = 0;
                    status = Sweep_set;
                    break;
                }
                case Sweep_adc:
                case Sweep_set:
                {
                    qDebug() << "RxData: Repeat the test run";
                    repeatTest = true;
                    break;
                }
                default:
                {
                    qDebug() << "ERROR: RxData: Cannot start from state:" << status_name[status];
                    break;
                }
            }
        }
    }

    // Instead of going to Idle, spoof the interrupted command response
    if (interrupted.cmd && status == Idle)
    {
        interrupted.cmd = false;
        RxCode = RXSUCCESS;
        response = interrupted.response;
        status = interrupted.status;
        qDebug() << "Return from interrupted state:" << status_name[status];
    }

    // ---------------------------------------------------
    // Main state machine
    // Check state
    qDebug() << "RxData: status:" << status << ":" << status_name[status];

    switch (status)
    {
        case send_ping:
        {
            heat = 0;
            SendEndMeasurementCommand(&CmdRsp);
            status = WaitPing;
            break;
        }
        case read_adc:
        {
            heat = 0;
            SendADCCommand(&CmdRsp);
            status = wait_adc;
            break;
        }
        case start_sweep_heater:
        {
            time = 0;

            StoreData(false); // Init data store
            VsStep = 0;
            VgStep = 0;
            VaStep = 0;
            curve = 0;

            ui->statusBar->showMessage("Heating setup");
            heat = 0;
            ui->HeaterProg->setValue(1);
            ui->CaptureProg->setValue(1);

            SendStartMeasurementCommand(&CmdRsp, lim[options.Ilimit], avg[options.AvgNum],
                                        Ir[options.IsRange], Ir[options.IaRange]);

            status = Heating_wait00;
            break;
        }
        case wait_stop:
        {
            if (RxCode == RXSUCCESS)
            {
                status = HeatOff;
            }
            break;
        }
        case Idle:
        {
            // All operations completed so stop
            StopTheMachine();
            break;
        }
        case WaitPing:
        {
            if (RxCode == RXSUCCESS)
            {
                status = Idle;
                ui->statusBar->showMessage("Ping OK");
            }
            break;
        }
        case wait_adc:
        {
            if (RxCode == RXSUCCESS)
            {
                saveADCInfo(&response);
                status = Idle;
                ui->statusBar->showMessage("Ready");
            }
            break;
        }
        case Heating_wait00:
        {
            if (RxCode == RXSUCCESS)
            {
                ui->statusBar->showMessage("Heating, get ADC info");
                SendADCCommand(&CmdRsp);
                status = Heating_wait_adc;
            }
            break;
        }
        case Heating_wait_adc:
        {
            if (RxCode == RXSUCCESS)
            {
                VgNow=ui->VgStart->text().toFloat();
                saveADCInfo(&response);
                heat = 0;
                status = Heating;
                ui->statusBar->showMessage("Heating");
                SendFilamentCommand(&CmdRsp, 0);
                heat = 1;
            }
            break;
        }
        case Heating:
        {
            if (RxCode == RXSUCCESS)
            {
                if (heat <= HEAT_CNT_MAX)
                {
                    ui->HeaterProg->setValue((100 * heat) / HEAT_CNT_MAX);
                    VfADC = GetVf((float)heat);
                    if (VfADC > 1023) VfADC = 1023;
                    SendFilamentCommand(&CmdRsp, (uint16_t)VfADC);
                    heat++;
                }
                else
                {
                    ui->HeaterProg->setValue(100);
                    status = heat_done;
                }
            }
            break;
        }
        case heat_done:
        {
            QString m = QString("Press 'start' when ready; heating for %1 secs").arg(time / 1000);
            time += TIMER_SET;
            ui->statusBar->showMessage(m);
            delay = options.Delay;
            if (time / 1000 == HEAT_WAIT_SECS)
            {
                if (dataStore->length() > 0) dataStore->clear();
                CreateTestVectors();
                curve = 0;
                status = Sweep_set;
            }
            break;
        }
        case Sweep_set:
        {
            if (delay == 0)
            {
                status=hold_ack;
                ui->statusBar->showMessage("Sweep Measure");
                VaNow = sweepList->at(curve).Va;
                VsNow = sweepList->at(curve).Vs;
                VgNow = sweepList->at(curve).Vg;
                VaADC = GetVa(VaNow);
                VsADC = GetVs(VsNow);
                VgADC = GetVg(VgNow);
                VfADC = GetVf( HEAT_CNT_MAX );
                status = Sweep_adc;
                SendGetMeasurementCommand(&CmdRsp, (uint16_t)VaADC, (uint16_t)VsADC, (uint16_t)VgADC, (uint16_t)VfADC);
            }
            else
            {
                ui->statusBar->showMessage("Sweep hold");
                VaNow = sweepList->at(curve).Va;
                VsNow = sweepList->at(curve).Vs;
                VgNow = sweepList->at(curve).Vg;
                if (VaNow > 425 || VsNow > 425)
                {
                    ui->statusBar->showMessage("Internal Error: Vaor Vs excessive");
                    status = HeatOff;
                    float v = VaNow > VsNow ? VaNow : VsNow;
                    distime = (int)(DISTIME * v / 400);
                    HV_Discharge_Timer = distime;
                    ui->statusBar->showMessage("Abort:Heater off");
                    SendFilamentCommand(&CmdRsp, 0);
                    heat=0;
                    break;
                }
                VaADC = GetVa(VaNow);
                VsADC = GetVs(VsNow);
                VgADC = GetVg(VgNow);
                VfADC = GetVf(HEAT_CNT_MAX);
                status = hold_ack;
                SendHoldMeasurementCommand(&CmdRsp, (uint16_t)VaADC, (uint16_t)VsADC, (uint16_t)VgADC, (uint16_t)VfADC, options.Delay);
            }
            break;
        }
        case hold_ack:
        {
            status = hold;
            break;
        }
        case hold:
        {
            if (delay == 0)
            {
                status = Sweep_adc;
                ui->statusBar->showMessage("Sweep (set measurement parameters)");
                VaNow = sweepList->at(curve).Va;
                VsNow = sweepList->at(curve).Vs;
                VgNow = sweepList->at(curve).Vg;
                if (VaNow > 425 || VsNow > 425)
                {
                    ui->statusBar->showMessage("Internal Error: Va or Vs excessive");
                    status = HeatOff;
                    float v = VaNow > VsNow ? VaNow : VsNow;
                    distime = (int)(DISTIME * v / 400);
                    HV_Discharge_Timer = distime;
                    ui->statusBar->showMessage("Abort:Heater off");
                    SendFilamentCommand(&CmdRsp, 0);
                    heat = 0;
                    break;
                }

                VaADC = GetVa(VaNow);
                VsADC = GetVs(VsNow);
                VgADC = GetVg(VgNow);
                VfADC = GetVf(HEAT_CNT_MAX);
                SendGetMeasurementCommand(&CmdRsp, (uint16_t)VaADC, (uint16_t)VsADC, (uint16_t)VgADC, (uint16_t)VfADC);
            }
            else
            {
                ui->statusBar->showMessage("Sweep (Holding)");
                delay--;
                status = hold;
            }
            break;
        }
        case Sweep_adc:
        {
            if (RxCode == RXSUCCESS)
            {
                //check current limit
                saveADCInfo(&response);
                StoreData(true); //Add to data store
                if (response.mid(0,2) == "11")
                {
                    ui->statusBar->showMessage("WARNING: Current Limit Hit");
                    qDebug() << "Sweep_adc: Current Limit Hit";
                    if (options.AbortOnLimit == true)
                    {
                        qDebug() << "Sweep_adc: Current Limit Abort";
                        status = HeatOff;
                        float v = VaNow > VsNow ? VaNow : VsNow;
                        distime =(int)(DISTIME * v / 400);
                        HV_Discharge_Timer =distime;
                        ui->CaptureProg->setValue(100);
                        SendFilamentCommand(&CmdRsp, 0);
                        break;
                    }
                }
                curve++;
                bool done = false;
                if ((power > tubeData.powerLim) && !ui->checkQuickTest->isChecked())
                {
                    while (!done)
                    {
                        if (sweepList->at(curve).Va >= VaNow)
                        {
                            results.Ia = -1; // mark as ignore
                            dataStore->append(results);
                            curve++; //skip high power points
                            if (curve == sweepList->length())
                            {
                                done = true;
                                if ((dataStore->length() > 5) && ui->TubeType->currentText() != NONE)
                                {
                                    optimizer->Optimize(dataStore, ui->TubeType->currentIndex(), 1, VaSteps, VsSteps, VgSteps);
                                    updateLcdsWithModel();
                                    RePlot(dataStore);
                                }
                            }
                        }
                        else done = true;

                    }
                }
                if (curve < sweepList->length())
                {
                    int progress = (100 * curve) / sweepList->length();
                    ui->CaptureProg->setValue(progress);
                    QString msg = QString("Sweep %1 % done").arg(progress);
                    ui->statusBar->showMessage(msg);
                    delay = options.Delay;
                    status = Sweep_set;
                }
                else
                {
                    if (repeatTest) //skip re-heating
                    {
                        repeatTest = false;
                        status = heat_done;
                        ui->statusBar->showMessage("Sweep complete");
                        ui->CaptureProg->setValue(100);
                    }
                    else
                    {
                        status = HeatOff;
                        float v = VaNow > VsNow ? VaNow : VsNow;
                        distime = (int)(DISTIME * v / 400);
                        HV_Discharge_Timer = distime;
                        ui->statusBar->showMessage("Sweep complete");
                        ui->CaptureProg->setValue(100);
                        SendFilamentCommand(&CmdRsp, 0);
                    }
                }
            }
            break;
        }
        case HeatOff:
        {
            if (RxCode != RXIDLE && RxCode != RXSUCCESS) break;

            if (HV_Discharge_Timer>0) HV_Discharge_Timer--;
            if (HV_Discharge_Timer == (distime-1))
            {
                ui->HeaterProg->setValue(0);
                SendEndMeasurementCommand(&CmdRsp);
                heat = 0;
                if (ui->checkAutoNumber->isChecked())
                {
                    if (!ui->checkQuickTest->isChecked()) on_actionSave_plot_triggered();
                    on_actionSave_Data_triggered();
                    int fn = ui->AutoNumber->text().toInt(&ok);
                    ui->AutoNumber->setText(QString::number(fn + 1));
                }
            }
            else if (HV_Discharge_Timer==0)
            {
                ui->statusBar->showMessage("Ready");
                status=Idle;
            }
            else
            {
                QString msg = QString("Countdown for HV to discharge: %1").arg(HV_Discharge_Timer);
                ui->statusBar->showMessage(msg);
            }
        }
        default:
        {
            qDebug() << "ERROR: unknown status:" << status;
            StopTheMachine();
            break;
        }
    }
}

// ------------------
// Plot management
// ------------------

// Initialise plot Area
/*
 void MainWindow::SetUpPlot() {
    //Set up plot
    //ui->PlotArea->addGraph();
    //X axis title,scale
    ui->PlotArea->xAxis->setLabel("Va");
    ui->PlotArea->xAxis->setRange(0,VaEnd);
    //Y1 axis title, scale
    ui->PlotArea->yAxis->setLabel("Ia mA");
    ui->PlotArea->yAxis->setRange(0,1);
    //Y2 axis title, scale
    ui->PlotArea->yAxis2->setLabel("Ia/g2 mA");
    ui->PlotArea->yAxis2->setRange(0,1);
    ui->PlotArea->yAxis2->setVisible(false);
    ui->PlotArea->legend->setVisible(ui->checkLegend->checkState());
    //Interactions
    ui->PlotArea->setInteraction(QCustomPlot::iSelectPlottables,true);
    ui->PlotArea->setInteraction(QCustomPlot::iRangeDrag,false);
    ui->PlotArea->setInteraction(QCustomPlot::iRangeZoom,false);
    connect(ui->PlotArea, SIGNAL(selectionChangedByUser()), this, SLOT(PlotInteractionUser()));
    //Title

    //repeat for plot 2
    //Set up plot
    //ui->PlotArea->addGraph();
    //X axis title,scale
    ui->PlotArea_2->xAxis->setLabel("Va");
    ui->PlotArea_2->xAxis->setRange(0,VaEnd);
    //Y1 axis title, scale
    ui->PlotArea_2->yAxis->setLabel("Ia mA");
    ui->PlotArea_2->yAxis->setRange(0,1);
    //Y2 axis title, scale
    ui->PlotArea_2->yAxis2->setLabel("Ia/g2 mA");
    ui->PlotArea_2->yAxis2->setRange(0,1);
    ui->PlotArea_2->yAxis2->setVisible(false);
    ui->PlotArea_2->legend->setVisible(ui->checkLegend->checkState());
    //Interactions
    ui->PlotArea_2->setInteraction(QCustomPlot::iSelectPlottables,true);
    ui->PlotArea_2->setInteraction(QCustomPlot::iRangeDrag,false);
    ui->PlotArea_2->setInteraction(QCustomPlot::iRangeZoom,false);
    //connect(ui->PlotArea_2, SIGNAL(selectionChangedByUser()), this, SLOT(PlotInteractionUser()));
    //Title
    UpdateTitle();

//    ui->PlotArea->setRangeDrag(Qt::Vertical);
//    ui->PlotArea->setRangeZoom(Qt::Vertical);
}
*/



// ======================
// Plot new point
// ======================

void MainWindow::RePlot(QList<results_t> * dataSet) {

    if (dataSet==0) return;
    if (dataSet->length() <2) return;
    activeStore = dataSet;
    int numPen = penList->length();
    for(int i=0; i<plotTabs.length();i++) {
       plot1.penList = penList;
       plot1.penChange=false;
       plot1.dataSet = dataSet;
       plot1.VaSteps =VaSteps;
       plot1.VsSteps =VsSteps;
       plot1.VgSteps =VgSteps;
       plot1.VaStart =VaStart;
       plot1.VaEnd = VaEnd;
       plot1.VsStart =VsStart;
       plot1.VsEnd = VsEnd;
       plot1.VgStart =VgStart;
       plot1.VgEnd = VgEnd;
       plot1.tube =  ui->TubeSelector->currentText();
       plot1.type = ui->TubeType->currentText();
       plotTabs.at(i)->plotUpdate(&plot1);
    }
    if ( plot1.penChange ) SaveCalFile();

}

/* /---------------------------------------------------
//Removed as preferred the look of the legend
//Annotate the plot with labels
void MainWindow::AddPlotLabels() {
    for(int g; g< VgSteps; g++) {
        for(int s; g< VsSteps; s++) {
            QCPItemText *textLabel = new QCPItemText(ui->PlotArea);
            ui->PlotArea->addItem(textLabel);
            textLabel->setPositionAlignment(Qt::AlignTop|Qt::AlignHCenter);
            textLabel->position->setType(QCPItemPosition::ptAxisRectRatio);
            results_t dataPoint = dataSet->at(a + s*(VaSteps+1) + g*(VsSteps+1)*(VaSteps+1));
            textLabel->position->setCoords(0.95, 0); // place position at center/top of axis rect
            textLabel->setText(QString("Vs Vg"));
            textLabel->setFont(QFont(font().family(), 10)); // make font a bit larger
            //textLabel->setPen(QPen(Qt::black)); // show black border around text
        }
    }
}
*/

/*void MainWindow::on_checkLegend_stateChanged(int arg1)
{
    //if (arg1==2) plotLegend=true; else plotLegend=false;
    ui->PlotArea->legend->setVisible(ui->checkLegend->checkState());
    if (activeStore != 0) ui->PlotArea->replot();
}
*/

//---------------------------------------------------
// Manage results data pool
void MainWindow::StoreData(bool add)
{
    if(!add) {   //deletes data Store
        dataStore->clear();
        for (int i=0;i< plotTabs.length(); i++ ){
            plotTabs.at(i)->clearGraphs();
        }
   }
   else
   {
        results.Ia=adc_real.Ia;
        results.Is=adc_real.Is;
        results.Va=adc_real.Va;
        results.Vs=adc_real.Vs;
        results.Vg=adc_real.Vg;
        results.Vf=adc_real.Vf;
        results.gm_a=0;
        results.gm_b=0;
        results.ra_a=0;
        results.ra_b=0;
        results.mu_a=0;
        results.mu_b=0;
        //in lieu of model data, copy Ia & Is
        results.IaMod=0;
        results.IsMod=0;
//        results.IaMod=results.Ia;
//        results.IsMod=results.Is;
        dataStore->append(results);
        ClearLCDs();
        ui->LCD1_label->setText("Va(a)");
        ui->LCD2_label->setText("Va(b)/g2");
        ui->LCD3_label->setText("Vg1");
        ui->LCD4_label->setText("Ia(a)");
        ui->LCD5_label->setText("Ia(b)/g2");
        ui->LCD6_label->setText("W");
        ui->lcd1->display(round(adc_real.Va * 10)/10);
        ui->lcd2->display(round(adc_real.Vs * 10)/10);
        ui->lcd3->display(round(adc_real.Vg * 10)/10);
        ui->lcd4->display(round(adc_real.Ia * 10)/10);
        ui->lcd5->display(round(adc_real.Is * 10)/10);

        if (adc_real.Ia<300) ui->lcd4->setPalette(Qt::green);
        else if (adc_real.Ia<350) ui->lcd4->setPalette(Qt::yellow);
        else ui->lcd4->setPalette(Qt::red);

        if (adc_real.Is<300) ui->lcd5->setPalette(Qt::green);
        else if (adc_real.Is<350) ui->lcd5->setPalette(Qt::yellow);
        else ui->lcd5->setPalette(Qt::red);

        power = adc_real.Ia * adc_real.Va/1000;
        ui->lcd6->display(round( power*10)/10);
        if ( power > tubeData.powerLim ) ui->lcd6->setPalette((Qt::red));
            else ui->lcd6->setPalette(Qt::green);
        //check for end of quicktest
        if (ui->checkQuickTest->isChecked()) {
            if (dataStore->length()==7) {
                QuickRes.clear();
                ClearLCDs();
                float Is_avg =0;
                float Ia_avg =0;
                for (int i=0; i<6; i++) {
                    Is_avg+=dataStore->at(i).Is;
                    Ia_avg+=dataStore->at(i).Ia;
                }
                Ia_avg/=6;
                Is_avg/=6;
                //mu, gm, Ia
                if (ui->TubeType->currentText()=="Koren Diode") {
                    ui->LCD1_label->setText("Ra(a)K");
                    ui->LCD2_label->setText("Ra(b)K");
                    ui->LCD3_label->setText("Ia");
                    float Ra = (dataStore->at(1).Va-dataStore->at(0).Va)/(dataStore->at(1).Ia-dataStore->at(0).Ia);
                    Ra =round(Ra*10)/10;
                    ui->lcd1->display( Ra );
                    float Rb = (dataStore->at(3).Vs-dataStore->at(2).Vs)/(dataStore->at(3).Is-dataStore->at(2).Is);
                    round(Rb*10)/10;
                    ui->lcd2->display(Rb);
                    ui->lcd3->display(Ia_avg);
                    //Emmission
                    ui->LCD8_label->setText("Ia(Emm)");
                    ui->lcd8->display( dataStore->at(6).Ia );
                    if ( dataStore->at(6).Ia < ui->EmmIa->text().toFloat() )
                        ui->lcd8->setPalette(Qt::red);
                    else
                        ui->lcd8->setPalette(Qt::green);

                    QTextStream(&QuickRes)  << "Ia(a)=" << Ia_avg
                                            << " Ia(b)=" << Is_avg
                                            << " Ra(a)=" << Ra
                                            << " Ra(b)=" << Rb
                                            << " IaEmm=" << dataStore->at(6).Ia;
                }
                else if (ui->TubeType->currentText()=="Koren Triode") {
                    ui->LCD1_label->setText("RaK");
                    ui->LCD2_label->setText("Gm-mS");
                    ui->LCD3_label->setText("Mu");
                    ui->LCD4_label->setText("Ia");
                    float Ra = (dataStore->at(1).Va-dataStore->at(0).Va)/(dataStore->at(1).Ia-dataStore->at(0).Ia);
                    float Gm = (dataStore->at(5).Ia-dataStore->at(4).Ia)/(dataStore->at(5).Vg-dataStore->at(4).Vg);
                    ui->lcd1->display( round(Ra*10)/10 );
                    ui->lcd2->display( Gm );
                    ui->lcd3->display( round(Ra*Gm*10)/10);
                    ui->lcd4->display(Ia_avg);

                    //Emmission
                    ui->LCD9_label->setText("Ia(Emm)");
                    ui->lcd9->display( dataStore->at(6).Ia );
                    if ( dataStore->at(6).Ia < ui->EmmIa->text().toFloat() )
                        ui->lcd9->setPalette(Qt::red);
                    else
                        ui->lcd9->setPalette(Qt::green);

                    QTextStream(&QuickRes) << "RaK=" << Ra
                                           << " Gm=" << Gm*10
                                           << " Mu=" << Ra*Gm
                                           << " Ia(a)=" << Ia_avg
                                           << " IaEmm=" << dataStore->at(6).Ia;
                }
                else if (ui->TubeType->currentText()=="Koren Dual Triode") {
                    ui->LCD1_label->setText("Ra K(a)");
                    ui->LCD2_label->setText(" Gm mS(a)");
                    ui->LCD3_label->setText(" Mu(a)");
                    ui->LCD4_label->setText(" Ia(a)");
                    ui->LCD5_label->setText(" Ra K(b)");
                    ui->LCD6_label->setText(" Gm mS(b)");
                    ui->LCD7_label->setText(" Mu(b)");
                    ui->LCD8_label->setText(" Ia(b)");
                    float Ra = (dataStore->at(1).Va-dataStore->at(0).Va)/(dataStore->at(1).Ia-dataStore->at(0).Ia);
                    float Gm = (dataStore->at(5).Ia-dataStore->at(4).Ia)/(dataStore->at(5).Vg-dataStore->at(4).Vg);
                    ui->lcd1->display( Ra );
                    ui->lcd2->display( Gm );
                    ui->lcd3->display( Ra*Gm);
                    float Rb = (dataStore->at(3).Vs-dataStore->at(2).Vs)/(dataStore->at(3).Is-dataStore->at(2).Is);
                    float Gmb = (dataStore->at(5).Is-dataStore->at(4).Is)/(dataStore->at(5).Vg-dataStore->at(4).Vg);
                    ui->lcd4->display(Ia_avg);
                    ui->lcd5->display( Rb );
                    ui->lcd6->display( Gmb);
                    ui->lcd7->display(Rb*Gmb);
                    ui->lcd8->display(Is_avg);
                    //Emmission
                    ui->LCD9_label->setText("Ia(Emm)");
                    ui->lcd9->display( dataStore->at(6).Ia );
                    if ( min(dataStore->at(6).Ia, dataStore->at(6).Is) < ui->EmmIa->text().toFloat() )
                        ui->lcd9->setPalette(Qt::red);
                    else
                        ui->lcd9->setPalette(Qt::green);

                    QTextStream(&QuickRes)
                        << "Ra(a)K" << "\t"
                        << "gm(a)" <<  "\t"
                        << "Mu(a)" <<  "\t"
                        << "Ia(a)" <<  "\t"
                        << "Ra(b)K" <<  "\t"
                        << "Gm(b)" <<  "\t"
                        << "Mu(b)" <<  "\t"
                        << " Ia(b)" <<  "\t"
                        << " IaEmm=" << "\n"
                        << Ra  << "\t"
                        << Gm  << "\t"
                        << Ra*Gm  << "\t"
                        << Ia_avg  << "\t"
                        << Rb << "\t"
                        << Gmb << "\t"
                        << Rb*Gmb << "\t"
                        << Is_avg << "\t"
                        << min(dataStore->at(6).Ia,dataStore->at(6).Is);
                }
                else if (ui->TubeType->currentText()!=NONE || ui->checkQuickTest->isChecked() ) {
                    //pentodes                    
                    ui->LCD1_label->setText("Ra K");
                    float Ra = (dataStore->at(1).Va-dataStore->at(0).Va)/(dataStore->at(1).Ia-dataStore->at(0).Ia);
                    ui->lcd1->display( round(Ra*10)/10 );

                    ui->LCD2_label->setText("Ia/Vg2 mS");
                    float Gm2 = (dataStore->at(3).Ia-dataStore->at(2).Ia)/(dataStore->at(3).Vs-dataStore->at(2).Vs);
                    ui->lcd2->display( Gm2 );

                    ui->LCD3_label->setText("Gm mS");
                    float Gm1 = (dataStore->at(5).Ia-dataStore->at(4).Ia)/(dataStore->at(5).Vg-dataStore->at(4).Vg);
                    ui->lcd3->display( Gm1 );

                    ui->LCD4_label->setText("Mu");
                    ui->lcd4->display( Ra*Gm1);

                    ui->LCD5_label->setText("Ia");
                    ui->lcd5->display(Ia_avg);

                    ui->LCD6_label->setText("Is");
                    ui->lcd6->display(Is_avg);

                    //Emmission
                    ui->LCD9_label->setText("Ia(Emm)");
                    ui->lcd9->display( dataStore->at(6).Ia );
                    if ( dataStore->at(6).Ia < ui->EmmIa->text().toFloat() )
                        ui->lcd9->setPalette(Qt::red);
                    else
                        ui->lcd9->setPalette(Qt::green);


                    QTextStream(&QuickRes)
                            << "Ra(a)K" << "\t"
                            << " gmG2" << "\t"
                            << " gmG1" << "\t"
                            << " Mu" << "\t"
                            << " Ia" << "\t"
                            << " Ig2" << "\t"
                            << " Ia Emmision" << "\n"
                            << Ra << "\t"
                            << Gm2 << "\t"
                            << Gm1 << "\t"
                            << Ra*Gm1 << "\t"
                            << Ia_avg << "\t"
                            << Is_avg << "\t"
                            << dataStore->at(6).Ia;

                }
            }
        }
        else if ( (dataStore->length()>5) && (dataStore->length()== sweepList->length()
                                              && ui->TubeType->currentText()!=NONE)) {
                optimizer->Optimize(dataStore, ui->TubeType->currentIndex(), 1, VaSteps, VsSteps, VgSteps);
                updateLcdsWithModel();
        }
        if (!ui->checkQuickTest->isChecked()) RePlot(dataStore);
    }
}

void MainWindow::ClearLCDs() {
    ui->lcd1->setPalette(Qt::green);
    ui->lcd2->setPalette(Qt::green);
    ui->lcd3->setPalette(Qt::green);
    ui->lcd4->setPalette(Qt::green);
    ui->lcd5->setPalette(Qt::green);
    ui->lcd6->setPalette(Qt::green);
    ui->lcd7->setPalette(Qt::green);
    ui->lcd8->setPalette(Qt::green);
    ui->lcd9->setPalette(Qt::green);

    ui->LCD1_label->setText("");
    ui->LCD2_label->setText("");
    ui->LCD3_label->setText("");
    ui->LCD4_label->setText("");
    ui->LCD5_label->setText("");
    ui->LCD6_label->setText("");
    ui->LCD7_label->setText("");
    ui->LCD8_label->setText("");
    ui->LCD9_label->setText("");

    ui->lcd1->display("--");
    ui->lcd2->display("--");
    ui->lcd3->display("--");
    ui->lcd4->display("--");
    ui->lcd5->display("--");
    ui->lcd6->display("--");
    ui->lcd7->display("--");
    ui->lcd8->display("--");
    ui->lcd9->display("--");
}

void MainWindow::updateLcdsWithModel() {

    float Ra,Gm;
    float Vca = ui->Vca->text().toFloat();
    float Vcs = ui->Vcs->text().toFloat();
    float Vcg = ui->Vcg->text().toFloat();

    ClearLCDs();

    // TDB : calculate correct values
    if (ui->TubeType->currentText()==DIODE) {
        ui->LCD1_label->setText("Ra(a) K");

        Ra = optimizer->RaCalc(Vca, Vcs, Vcg);
        ui->lcd1->display(round(Ra*10)/10 );
    }
    else if (ui->TubeType->currentText()==DUAL_DIODE) {
        ui->LCD1_label->setText("Ra(a) K");
        ui->LCD2_label->setText("Ra(b) K");
        Ra = optimizer->RaCalc(Vca, Vcs, Vcg);
        ui->lcd1->display(round(Ra*10)/10 );
        Ra = optimizer->RaCalcB(Vca, Vcs, Vcg);
        ui->lcd2->display(round(Ra*10)/10 );
    }
    else if (ui->TubeType->currentText()==TRIODE) {
        ui->LCD1_label->setText("Ra K");
        ui->LCD2_label->setText("Gm mS");
        ui->LCD3_label->setText("Mu");

        Ra = optimizer->RaCalc(Vca, Vcs, Vcg);
        Gm = optimizer->GmCalc(Vca, Vcs, Vcg);
        ui->lcd1->display( round(Ra*10)/10 );
        ui->lcd2->display( round(Gm*10)/10 );
        ui->lcd3->display( round(Ra*Gm*10)/10);
    }
    else if (ui->TubeType->currentText()==DUAL_TRIODE) {
        ui->LCD1_label->setText("Ra(a) K");
        ui->LCD2_label->setText("Gm(a) mS(a)");
        ui->LCD3_label->setText("Mu(a)");
        ui->LCD4_label->setText("Ra(b) K");
        ui->LCD5_label->setText("Gm(b) mS");
        ui->LCD6_label->setText("Mu(b)");

        Ra = optimizer->RaCalc(Vca, Vcs, Vcg);
        Gm = optimizer->GmCalc(Vca, Vcs, Vcg);
        ui->lcd1->display( round(Ra*10)/10 );
        ui->lcd2->display( round(Gm*10)/10 );
        ui->lcd3->display( round(Ra*Gm*10)/10);

        Ra = optimizer->RaCalcB(Vca, Vcs, Vcg);
        Gm = optimizer->GmCalcB(Vca, Vcs, Vcg);
        ui->lcd4->display( round(Ra*10)/10 );
        ui->lcd5->display( round(Gm*10)/10 );
        ui->lcd6->display( round(Ra*Gm*10)/10);
    }
    else if (ui->TubeType->currentText()!=NONE) {
        //pentodes
        ui->LCD1_label->setText("Ra K");
        ui->LCD2_label->setText("Gm mS");
        ui->LCD3_label->setText("Mu");
        Ra = optimizer->RaCalc(Vca, Vcs, Vcg);
        Gm = optimizer->GmCalc(Vca, Vcs, Vcg);
        ui->lcd1->display( round(Ra*10)/10 );
        ui->lcd2->display( round(Gm*10)/10 );
        ui->lcd3->display( round(Ra*Gm*10)/10);
    }
}
//---------------------------------------------------
// Transform ADC values into real data
void MainWindow::saveADCInfo(QByteArray * adc_pkt)
{
    adc_data.Ia = adc_pkt->mid(2,4).toInt(&ok,16);
    adc_data.Ia_raw = adc_pkt->mid(6,4).toInt(&ok,16);
    adc_data.Is = adc_pkt->mid(10,4).toInt(&ok,16);
    adc_data.Is_raw = adc_pkt->mid(14,4).toInt(&ok,16);
    adc_data.Va = adc_pkt->mid(18,4).toInt(&ok,16);
    adc_data.Vs = adc_pkt->mid(22,4).toInt(&ok,16);
    adc_data.Vsu = adc_pkt->mid(26,4).toInt(&ok,16);
    adc_data.Vn = adc_pkt->mid(30,4).toInt(&ok,16);
    adc_data.Gain_a = adc_pkt->mid(34,2).toInt(&ok,16);
    adc_data.Gain_s = adc_pkt->mid(36,2).toInt(&ok,16);
    //qDebug() << "Ia_gain_pkt=" << adc_pkt->mid(34,2) << "Is_gain_pkt=" << adc_pkt->mid(36,2);
    GetReal();
}

void MainWindow::GetReal()
{   float gain[8]={1, 2, 5, 10, 20, 50, 100, 200};
    float avg[8]={0,1,2,4,8,16,32,1}; //0 represents auto averaging
    float Iares=18;
    float Isres=18;

    //determine averaging used based on highest gain
    float auto_avg[8]={1,1,1,1,2,2,4,8};
    float av = avg[options.AvgNum];
    int a = adc_data.Gain_a;
    if ( a < adc_data.Gain_s) a = adc_data.Gain_s;
    if (options.AvgNum==0) av = auto_avg[a];

    // calc  actual currents
    int i =adc_data.Gain_a;
    if (i>7) i=7;
    if (calData.IaMax==400) {
        adc_real.Ia = (float) (adc_data.Ia) /gain[i] * adc_scale.Ia400 * calData.IaVal/av ;
        adc_real.Ia_raw = (float) adc_data.Ia_raw * adc_scale.Ia400_raw/av;
        Iares=9;
    } else {
        adc_real.Ia = (float) (adc_data.Ia) /gain[i] * adc_scale.Ia * calData.IaVal/av ;
        adc_real.Ia_raw = (float) adc_data.Ia_raw * adc_scale.Ia_raw/av;
    }

    i = adc_data.Gain_s;
    if (i>7) i=7;
    if (calData.IsMax==400) {
        adc_real.Is = ((float) (adc_data.Is)) /gain[i]* adc_scale.Is400 * calData.IsVal/av;
        adc_real.Is_raw = (float) adc_data.Is_raw * adc_scale.Is400_raw/av;
        Isres=9;
    } else {
        adc_real.Is = ((float) (adc_data.Is)) /gain[i]* adc_scale.Is * calData.IsVal/av;
        adc_real.Is_raw = (float) adc_data.Is_raw * adc_scale.Is_raw/av;
    }
    //Calc actual voltages
    adc_real.Vsu = ((float) adc_data.Vsu) * adc_scale.Vsu * calData.VsuVal;
    adc_real.Vg = VgNow;
    //Iares=0;
    //Isres=0;
    adc_real.Vn = 5.0*(adc_scale.Vn *(adc_data.Vn/1023.0-1.0)+1.0) * calData.VnVal;
    adc_real.Va = (float) adc_data.Va*5.0/1023.0 * adc_scale.Va * calData.VaVal -adc_real.Vsu + Vdi - Vdar + adc_real.Ia*Iares/1000.0;
    adc_real.Vs = (float) adc_data.Vs*5.0/1023.0 * adc_scale.Vs * calData.VsVal -adc_real.Vsu + Vdi - Vdar + adc_real.Is*Isres/1000.0;
    if (adc_real.Va < 1) {
        adc_real.Va =0;
        adc_real.Ia =0;
    }
    if (adc_real.Vs < 1) {
        adc_real.Vs =0;
        adc_real.Is =0;
    }
}

//These calculate the values to be sent to the uTracer
int MainWindow::GetVa(float v)
{   int x;
    x= (int)(1023.0/5.0*( (v + adc_real.Vsu -Vdi +Vdar)/adc_scale.Va/calData.VaVal ));
    return (x>1023?1023:x);
}
int MainWindow::GetVs(float v)
{
    int x;
    x = (int)(1023.0/5.0*((v + adc_real.Vsu -Vdi +Vdar)/ adc_scale.Vs / calData.VsVal));
    return (x>1023?1023:x);
}
int MainWindow::GetVg(float v)
{   float Vg,V0,V1;
    float VgScaled = v / options.VgScale;
    if (VgScaled >= -1) { // 0 > V > -1
        Vg=-1024 * VgScaled /calData.VgMax *calData.Vg1Val;
    } else if (VgScaled >= -4) { // -1 > V > -4
        V0 = -1024 /calData.VgMax *calData.Vg1Val;
        V1 = -1024 * 4 /calData.VgMax *calData.Vg4Val;
        Vg =(V1-V0)*(VgScaled-1)/(4-1) + V0;
    } else { //-4 > V
        V0 = -1024 /calData.VgMax * 4* calData.Vg4Val;
        V1 = -1024 * 40 /calData.VgMax *calData.Vg40Val;
        Vg =(V1-V0)*(VgScaled-4)/(40-4) + V0;
    }
    //Vg = -1024.0/50.0 * v *calData.Vg40Val + e;
    //Vg+=2;
    return  (int) (Vg>1023?1023:Vg);
}
int MainWindow::GetVf(float v)
{
    VfNow = v*ui->VhVal->text().toFloat()/HEAT_CNT_MAX;
    adc_real.Vf=  VfNow;
    // note it's rms voltage that matters!!
    return (int)(1024.0*(VfNow/adc_real.Vsu)*(VfNow/adc_real.Vsu));
}

//------------------------------------------------------
// Calibration File Management
bool MainWindow::ReadCalibration()
{
    // Start with the default calibration values
    // Default to the first serial port (if present)
    int i = 0;
    int firstEntry = -1;
    QList<QSerialPortInfo> serPortInfo = QSerialPortInfo::availablePorts();
    foreach (QSerialPortInfo port, serPortInfo)
    {
        if (!port.portName().isEmpty())
        {
            if (firstEntry == -1) firstEntry = i;
            break;
        }
        i++;
    }

    if (firstEntry != -1)
    {
        // Use the first available serial port
        comport = serPortInfo.at(firstEntry).portName();
    }
    else
    {
        // Otherwise indicate no serial port
        comport = "none";
    }

    // These are the calibrations values including comport.
    calData.VaVal = 1.0;
    calData.VsVal = 1.0;
    calData.IaVal = 1.0;
    calData.IsVal = 1.0;
    calData.VsuVal = 1.0;
    calData.Vg1Val = 1.0;
    calData.Vg4Val = 1.0;
    calData.Vg40Val = 1.0;
    calData.VnVal = 1.0;
    calData.RaVal = 6800;
    calData.VaMax = 300;
    calData.IaMax = 200;
    calData.IsMax = 200;
    calData.VgMax = 50;

    // Initialise the Pen List colours
    int r, g, b, h, s, v;
    QPen grPen;
    for (int i = 0; i < 16; i++)
    {
        h = (360 * (i % 16)) / 16;
        s = 200;
        v = 240 - 100 * (i / 16);
        r = QColor::fromHsv(h, s, v, 255).red();
        g = QColor::fromHsv(h, s, v, 255).green();
        b = QColor::fromHsv(h, s, v, 255).blue();
        grPen.setColor(QColor::fromRgb(r, g, b));
        grPen.setWidthF(2);
        penList->append(grPen);
    }

    // Check to see if the calibration file exists
    QFile datafile(calFileName);
    qDebug() << "Default calibration filename:" << calFileName;
    if (!datafile.exists())
    {
        // Create a default calibration file
        if (!SaveCalFile())
            return(false);
    }

    // Create a black pen to fill unused entries
    QPen black;
    black.setColor(QColor::fromRgb(0,0,0,255));
    black.setWidthF(2);

    // Read the calibration file which will replace the default settings in memory
    if (!datafile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "ERROR: failed to open for reading:" << calFileName;
        return(false);
    }
    QTextStream in(&datafile);
    while ( !in.atEnd() )
    {
        QString line = in.readLine();
        QStringList list = line.split("=");
        //qDebug() << "ReadCalibration:" << list;
        //Get Cal data and set sliders
        if (list.first()=="COM") comport=list.at(1);
        else if (list.first()=="Va") calData.VaVal = list.at(1).toFloat();
        else if (list.first()=="Vs") calData.VsVal = list.at(1).toFloat();
        else if (list.first()=="Ia") calData.IaVal = list.at(1).toFloat();
        else if (list.first()=="Is") calData.IsVal = list.at(1).toFloat();
        else if (list.first()=="Vsu") calData.VsuVal = list.at(1).toFloat();
        else if (list.first()=="Vg1") calData.Vg1Val = list.at(1).toFloat();
        else if (list.first()=="Vg4") calData.Vg4Val = list.at(1).toFloat();
        else if (list.first()=="Vg40") calData.Vg40Val = list.at(1).toFloat();
        else if (list.first()=="Vn") calData.VnVal = list.at(1).toFloat();
        else if (list.first()=="RaVal") calData.RaVal = list.at(1).toFloat();
        else if (list.first()=="VaMax") calData.VaMax = list.at(1).toFloat();
        else if (list.first()=="IaMax") calData.IaMax = list.at(1).toFloat();
        else if (list.first()=="IsMax") calData.IsMax = list.at(1).toFloat();
        else if (list.first()=="VgMax") calData.VgMax = list.at(1).toFloat();
        else if (list.first()=="PenNRGBWS") {
            QString ps = QString(list.at(1));
            QStringList psl = ps.split(",");
            grPen.setColor(QColor::fromRgb(psl.at(1).toInt(),psl.at(2).toInt(),psl.at(3).toInt()));
            grPen.setWidthF(psl.at(4).toInt());
            int n = psl.at(0).toInt();
            for(int i=0; i <= n; i++) {
                //Extend list if necessary, using the black pen
                if (penList->length() < (i+1)) penList->append(black);
            }
            penList->replace(n,grPen);
        }
    }
    datafile.close();
    QString m;
    QTextStream(&m) << "uTracer range ";
    ui->labelInfo_1->setText(m);
    m.clear();
    QTextStream(&m) << (int)calData.VaMax << "V Max";
    ui->labelInfo_2->setText(m);
    m.clear();
    QTextStream(&m) << calData.IaMax << "mA Max";
    ui->labelInfo_3->setText(m);

    adc_scale.Va=(470000 + calData.RaVal)/calData.RaVal;
    adc_scale.Vs=(470000 + calData.RaVal)/calData.RaVal;

    return(true);
}

bool MainWindow::SaveCalFile()
{
    qDebug () << "SaveCalFile...";
    bool ret = false;
    QFile datafile(calFileName);
    if (datafile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream calFile(&datafile);
        calFile << "COM=" << comport << "\n";
        calFile << "Va=" << calData.VaVal << "\n";
        calFile << "Vs=" << calData.VsVal << "\n";
        calFile << "Ia=" << calData.IaVal << "\n";
        calFile << "Is=" << calData.IsVal << "\n";
        calFile << "Vsu=" << calData.VsuVal << "\n";
        calFile << "Vg1=" << calData.Vg1Val << "\n";
        calFile << "Vg4=" << calData.Vg4Val << "\n";
        calFile << "Vg40=" << calData.Vg40Val << "\n";
        calFile << "Vn=" << calData.VnVal << "\n";
        calFile << "RaVal=" << calData.RaVal << "\n";
        calFile << "VaMax=" << calData.VaMax << "\n";
        calFile << "IaMax=" << calData.IaMax << "\n";
        calFile << "IsMax=" << calData.IsMax << "\n";
        calFile << "VgMax=" << calData.VgMax << "\n";
        calFile << ";Define pen number,color RGB, and width\n";
        int r,g,b,w;
        for(int i=0; i<penList->length(); i++) {
            //check if we
            r = penList->at(i).color().red() ;
            g = penList->at(i).color().green() ;
            b = penList->at(i).color().blue() ;
            w = penList->at(i).width() ;
            calFile << "PenNRGBWS="<< i << "," << r << "," << g << "," << b << "," << w << "\n";
        }
        calFile.flush();
        datafile.close();
        ret = true;
        //qDebug() << "SaveCalFile: ...updated cal file";
    } else qDebug() << "Save Cal file failed";

    return(ret);
}

//------------------------------------------------------
// Tube Data file Management
bool MainWindow::ReadDataFile()
{
    //Check to see if usual data file exists
    QFile datafile(dataFileName);
    qDebug() << "Default dataFileName:" << dataFileName;
    if (! datafile.exists(dataFileName)) {
        //It doesn't exist so ask for it
        dataFileName = QString();
        dataFileName = QFileDialog::getOpenFileName(this,tr("Read Tube Data file"),QDir::homePath(),"Text (*.csv)");
    }
    if (dataFileName.isNull()) {
        qDebug() << "ERROR: Valve database .csv file not specified";
        return(false);
    }
    qDebug() << "Using dataFileName: " << dataFileName;
    datafile.setFileName(dataFileName);
    datafile.open(QIODevice::ReadOnly | QIODevice::Text);
    QTextStream in(&datafile);
    QString line = in.readLine();
    QStringList dataline = line.split(",");
    //qDebug() << "MainWindow" << dataline;

    //read entire file into tubeDataList
    //The list is assumed to be sorted
    bool ok;
    while ( !in.atEnd() )
    {
        //Build list of tubes
        line = in.readLine();
        //qDebug() << "ReadDataFile:" << line;
        dataline = line.split(",");
        if (dataline.length()==20) {
            int i = 0;
            tubeData.ID=dataline.at(i++);
            tubeData.Anode =dataline.at(i++) ;
            tubeData.G2=dataline.at(i++);
            tubeData.G1=dataline.at(i++);
            tubeData.G1b=dataline.at(i++);
            tubeData.Cathode=dataline.at(i++);
            tubeData.G3=dataline.at(i++);
            tubeData.HtrA=dataline.at(i++);
            tubeData.HtrB=dataline.at(i++);
            tubeData.HtrVolts=dataline.at(i++).toFloat(&ok);
            tubeData.Model=dataline.at(i++).toInt(&ok);
            tubeData.VaStart=dataline.at(i++).toInt(&ok);
            tubeData.VaEnd=dataline.at(i++).toInt(&ok);
            tubeData.VaStep=dataline.at(i++).toInt(&ok);
            tubeData.VsStart=dataline.at(i++).toInt(&ok);
            tubeData.VsEnd=dataline.at(i++).toInt(&ok);
            tubeData.VsStep=dataline.at(i++).toInt(&ok);
            tubeData.VgStart=dataline.at(i++).toInt(&ok);
            tubeData.VgEnd=dataline.at(i++).toInt(&ok);
            tubeData.VgStep=dataline.at(i++).toInt(&ok);
            tubeData.Vca= 200;
            tubeData.Dva= 10;
            tubeData.Vcs= 200;
            tubeData.Dvs= 10;
            tubeData.Vcg= -5;
            tubeData.Dvg= 10;
            tubeData.powerLim=10;
            tubeData.EmmVa=200;
            tubeData.EmmVs=200;
            tubeData.EmmVg=0;
            tubeData.EmmIa=10;

            // add to library
            tubeDataList->append(tubeData);
            //qDebug() << "ReadDataFile: Added Line";
            if (dataline.first().length()>2) {
               ui->TubeSelector->addItem(dataline.first());
            }
        }
        else if (dataline.length()>=26)
        {
            int i = 0;
            tubeData.ID=dataline.at(i++);
            tubeData.Anode =dataline.at(i++) ;
            tubeData.G2=dataline.at(i++);
            tubeData.G1=dataline.at(i++);
            tubeData.G1b=dataline.at(i++);
            tubeData.Cathode=dataline.at(i++);
            tubeData.G3=dataline.at(i++);
            tubeData.HtrA=dataline.at(i++);
            tubeData.HtrB=dataline.at(i++);
            tubeData.HtrVolts=dataline.at(i++).toFloat(&ok);
            tubeData.Model=dataline.at(i++);
            tubeData.VaStart=dataline.at(i++).toFloat(&ok);
            tubeData.VaEnd=dataline.at(i++).toFloat(&ok);
            tubeData.VaStep=dataline.at(i++).toInt(&ok);
            tubeData.VsStart=dataline.at(i++).toFloat(&ok);
            tubeData.VsEnd=dataline.at(i++).toFloat(&ok);
            tubeData.VsStep=dataline.at(i++).toInt(&ok);
            tubeData.VgStart=dataline.at(i++).toFloat(&ok);
            tubeData.VgEnd=dataline.at(i++).toFloat(&ok);
            tubeData.VgStep=dataline.at(i++).toInt(&ok);
            tubeData.Vca=dataline.at(i++).toFloat(&ok);
            tubeData.Dva=dataline.at(i++).toFloat(&ok);
            tubeData.Vcs=dataline.at(i++).toFloat(&ok);
            tubeData.Dvs=dataline.at(i++).toFloat(&ok);
            tubeData.Vcg=dataline.at(i++).toFloat(&ok);
            tubeData.Dvg=dataline.at(i++).toFloat(&ok);
            tubeData.powerLim=10;
            tubeData.EmmVa=200;
            tubeData.EmmVs=200;
            tubeData.EmmVg=0;
            tubeData.EmmIa=10;

            if (dataline.length()>=27)  tubeData.powerLim=dataline.at(i++).toFloat(&ok);
            if (dataline.length()==31) {
                tubeData.EmmVa=dataline.at(i++).toFloat(&ok);
                tubeData.EmmVs=dataline.at(i++).toFloat(&ok);
                tubeData.EmmVg=dataline.at(i++).toFloat(&ok);
                tubeData.EmmIa=dataline.at(i++).toFloat(&ok);
            }
            // add to library
            tubeDataList->append(tubeData);
            //qDebug() << "ReadDataFile: Added Line";
            if (dataline.first().length()>2) {
               ui->TubeSelector->addItem(dataline.first());
            }
            else ui->statusBar->showMessage("Invalid line found in data.csv file");
        }
    }
    datafile.close();
    ui->TubeSelector->setCurrentIndex(0);
    on_TubeSelector_currentIndexChanged(tubeDataList->at(0).ID);
    //LabelPins(tubeDataList->at(0)); // should be triggered by change of index;
    return(true);
}

void MainWindow::LabelPins(tubeData_t tubeData) {
    bool ok;

    ui->AnodePin->setText(tubeData.Anode );
    ui->G2Pin->setText(tubeData.G2);
    ui->G1Pin->setText(tubeData.G1 );
    ui->G1bPin->setText(tubeData.G1b );
    ui->CathodePin->setText(tubeData.Cathode );
    ui->G3Pin->setText(tubeData.G3 );
    ui->HtraPin->setText(tubeData.HtrA );
    ui->HtrbPin->setText(tubeData.HtrB );
    ui->VhVal->setText(QString::number(tubeData.HtrVolts)) ;
    ui->TubeType->setCurrentIndex( ui->TubeType->findText(tubeData.Model) ) ;
    ui->VaStart->setText(QString::number(tubeData.VaStart ));
    ui->VaEnd->setText(QString::number(tubeData.VaEnd));
    ui->VaSteps->setText(QString::number(tubeData.VaStep ));
    ui->VsStart->setText(QString::number(tubeData.VsStart ));
    ui->VsEnd->setText(QString::number(tubeData.VsEnd ));
    ui->VsSteps->setText(QString::number(tubeData.VsStep ));
    ui->VgStart->setText(QString::number(tubeData.VgStart ));
    ui->VgEnd->setText(QString::number(tubeData.VgEnd ));
    ui->VgSteps->setText(QString::number(tubeData.VgStep ));
    //quick sweep settings
    ui->Vca->setText(QString::number(tubeData.Vca ));
    ui->DeltaVa->setText(QString::number(tubeData.Dva ));
    ui->Vcs->setText(QString::number(tubeData.Vcs ));
    ui->DeltaVs->setText(QString::number(tubeData.Dvs ));
    ui->Vcg->setText(QString::number(tubeData.Vcg ));
    ui->DeltaVg->setText(QString::number(tubeData.Dvg ));
    ui->PowLim->setText(QString::number(tubeData.powerLim ));
    ui->EmmVa->setText(QString::number(tubeData.EmmVa ));
    ui->EmmVs->setText(QString::number(tubeData.EmmVs ));
    ui->EmmVg->setText(QString::number(tubeData.EmmVg ));
    ui->EmmIa->setText(QString::number(tubeData.EmmIa ));
}

void MainWindow::on_TubeSelector_currentIndexChanged(const QString &arg1) {
 /*   QFile datafile(dataFileName);

    datafile.open(QIODevice::ReadOnly | QIODevice::Text);
    QTextStream in(&datafile);
    QString line = in.readLine(); //Discard header
    line = in.readLine(); //First data line
    QStringList dataline = line.split(",");
*/
    //look for matching ID
    int count = 0;
    while (arg1 != tubeDataList->at(count).ID && count<tubeDataList->length()) count++;

    ui->AutoFileName->setText( ui->TubeSelector->currentText());
    ui->AutoNumber->setText("1");
    tubeData = tubeDataList->at(count);
    LabelPins(tubeDataList->at(count));
    RePlot(activeStore);
}

//Add Tube type
void MainWindow::on_AddType_clicked()
{
    DataSaveDialog *w = new DataSaveDialog(this, ui->TubeSelector->currentText());
    w->setModal(true);
    w->show();
}

void MainWindow::DataSaveDialog_clicked(const QString &Type) {
    //copy current settings into tubeData
    tubeData.ID = Type;
    tubeData.Anode = ui->AnodePin->text()  ;
    tubeData.G2 = ui->G2Pin->text();
    tubeData.G1 = ui->G1Pin->text();
    tubeData.G1b = ui->G1bPin->text();
    tubeData.Cathode = ui->CathodePin->text();
    tubeData.G3 = ui->G3Pin->text();
    tubeData.HtrA = ui->HtraPin->text();
    tubeData.HtrB = ui->HtrbPin->text();
    tubeData.HtrVolts = ui->VhVal->text().toFloat();
    tubeData.Model = ui->TubeType->currentText();
    tubeData.VaStart = ui->VaStart->text().toFloat();
    tubeData.VaEnd = ui->VaEnd->text().toFloat();
    tubeData.VaStep = ui->VaSteps->text().toInt();
    tubeData.VsStart = ui->VsStart->text().toFloat();
    tubeData.VsEnd = ui->VsEnd->text().toFloat();
    tubeData.VsStep = ui->VsSteps->text().toInt();
    tubeData.VgStart = ui->VgStart->text().toFloat();
    tubeData.VgEnd = ui->VgEnd->text().toFloat();
    tubeData.VgStep = ui->VgSteps->text().toInt();
    tubeData.Vca = ui->Vca->text().toFloat();
    tubeData.Dva = ui->DeltaVa->text().toFloat();
    tubeData.Vcs = ui->Vcs->text().toFloat();
    tubeData.Dvs = ui->DeltaVs->text().toFloat();
    tubeData.Vcg = ui->Vcg->text().toFloat();
    tubeData.Dvg = ui->DeltaVg->text().toFloat();
    tubeData.powerLim = ui->PowLim->text().toFloat();
    tubeData.EmmVa = ui->EmmVa->text().toFloat();
    tubeData.EmmVs = ui->EmmVs->text().toFloat();
    tubeData.EmmVg = ui->EmmVg->text().toFloat();
    tubeData.EmmIa = ui->EmmIa->text().toFloat();

    //decide what to do with the copied data
    //qDebug() << "Changing tube=" << Type;
    int index = ui->TubeSelector->currentIndex();
    if (Type!="") {
        //qDebug() << "Changing tube data";
        if (Type != ui->TubeSelector->currentText()) {
            //Add new type so find correct alphabetic position
            bool done = false;
            int i = 0;
            while ((i< tubeDataList->length()-1) && !done ){

                //search for a slot
                //qDebug() << "Sorting:" << tubeDataList->at(i).ID <<(tubeDataList->at(i).ID > Type);
                if ( (tubeDataList->at(i).ID != Type) ) {
                    if ( (tubeDataList->at(i).ID > Type) ) {
                        tubeDataList->insert(i,tubeData);
                        ui->TubeSelector->insertItem(i,Type);
                        ui->TubeSelector->setCurrentIndex(i);
                        done = true;
                        //qDebug() << "Inserted new tube data";

                    }
                    i++;
                } else {
                    ui->TubeSelector->setCurrentIndex(i);
                    tubeDataList->replace(i,tubeData);
                    done = true;
                }
            }
            //if we didn't find a slot then append
            if (!done) {
                //qDebug() << "Appended new tube to database";
                tubeDataList->append(tubeData);
                //qDebug() << "Appended new type to selector";
                ui->TubeSelector->addItem(Type);
                ui->TubeSelector->setCurrentIndex(i);
            }
        }
        else {
            //qDebug() << "Updated existing new tube data";
            tubeDataList->replace(ui->TubeSelector->currentIndex(),tubeData);
        }
        //qDebug() << "Saving tube data file";
        SaveTubeDataFile();

    }
}

//Delete current Type type
void MainWindow::on_DeleteType_clicked()
{
    tubeDataList->removeAt(ui->TubeSelector->currentIndex());
    ui->TubeSelector->removeItem(ui->TubeSelector->currentIndex());
    SaveTubeDataFile();
}

bool MainWindow::SaveTubeDataFile() {

    QFile datafile(dataFileName);
    if (datafile.open(QIODevice::WriteOnly  | QIODevice::Text)) {
        QTextStream out(&datafile);

        out << "ID,A,G2,G1,G1b,Cathode,G3,Heater,Heater,Htr Volts,SPICE model,VaStart,VaEnd,VaStep,VsStart," <<
               "VsEnd,VsStep,VgStart,VgEnd,VgStep,Vca,Dva,Vcg2,Dvg2,Vcg,Dvg,Power,EmmVa,EmmVs,EmmVg,EmmIa\n";
        for (int i=0; i< tubeDataList->length(); i++) {
            out << tubeDataList->at(i).ID << ","
                << tubeDataList->at(i).Anode << ","
                << tubeDataList->at(i).G2 << ","
                << tubeDataList->at(i).G1 << ","
                << tubeDataList->at(i).G1b << ","
                << tubeDataList->at(i).Cathode << ","
                << tubeDataList->at(i).G3 << ","
                << tubeDataList->at(i).HtrA << ","
                << tubeDataList->at(i).HtrB << ","
                << tubeDataList->at(i).HtrVolts << ","
                << tubeDataList->at(i).Model << ","
                << tubeDataList->at(i).VaStart << ","
                << tubeDataList->at(i).VaEnd << ","
                << tubeDataList->at(i).VaStep << ","
                << tubeDataList->at(i).VsStart << ","
                << tubeDataList->at(i).VsEnd << ","
                << tubeDataList->at(i).VsStep << ","
                << tubeDataList->at(i).VgStart << ","
                << tubeDataList->at(i).VgEnd << ","
                << tubeDataList->at(i).VgStep << ","
                << tubeDataList->at(i).Vca << ","
                << tubeDataList->at(i).Dva << ","
                << tubeDataList->at(i).Vcs << ","
                << tubeDataList->at(i).Dvs << ","
                << tubeDataList->at(i).Vcg << ","
                << tubeDataList->at(i).Dvg << ","
                << tubeDataList->at(i).powerLim << ","
                << tubeDataList->at(i).EmmVa << ","
                << tubeDataList->at(i).EmmVs << ","
                << tubeDataList->at(i).EmmVg << ","
                << tubeDataList->at(i).EmmIa << "\n";
        }
        datafile.close();
        return(true);
    }
    return(true);
}
void MainWindow::on_TubeType_currentIndexChanged(int index)
{
    tubeData.Model=ui->TubeType->currentText();
    if (activeStore) {
        optimizer->Optimize(activeStore, ui->TubeType->currentIndex(), 1, VaSteps, VsSteps, VgSteps);
        updateLcdsWithModel();
        RePlot(activeStore);
    }
    if (ui->TubeType->currentText()==DUAL_TRIODE || ui->TubeType->currentText()==DUAL_DIODE) {
        ui->AnodeLabel->setText("A(a)");
        ui->Grid2Label->setText("A(b)");
        ui->Grid1Label->setText("G1(a)");
        ui->Grid1bLabel->setText("G1(b)");
        ui->Grid3Label ->setText("C(a)");
        ui->CathodeLabel->setText("C(b)");
    } else {
        ui->AnodeLabel->setText("A");
        ui->Grid2Label->setText("G2");
        ui->Grid1Label->setText("G1(a)");
        ui->Grid1bLabel->setText("G1(b)");
        ui->Grid3Label ->setText("G3");
        ui->CathodeLabel->setText("C");
    }
    updateTubeGreying();
}

void MainWindow::updateTubeGreying() {
    if (!ui->checkQuickTest->isChecked()) {
        //if triode or Diode then Vs=Va
        if (ui->TubeType->currentText()==TRIODE || ui->TubeType->currentText()==DUAL_TRIODE) {
            ui->checkVsEqVa->setChecked(true);
            ui->VsStart->setDisabled(true);
            ui->VsEnd->setDisabled(true);
            ui->VsSteps->setDisabled(true);
        } else if ( (ui->TubeType->currentText()==DIODE) || (ui->TubeType->currentText()==DUAL_DIODE)) {
            ui->VgSteps->setText("0");
            ui->VgSteps->setDisabled(true);
            ui->VgStart->setDisabled(true);
            ui->VgEnd->setDisabled(true);
            ui->VsSteps->setDisabled(true);
            ui->VsStart->setDisabled(true);
            ui->VsEnd->setDisabled(true);
        }
        else {
            ui->checkVsEqVa->setChecked(false);
            ui->VsStart->setDisabled(false);
            ui->VsEnd->setDisabled(false);
            ui->VsSteps->setDisabled(false);
        }
    }
}

void MainWindow::updateSweepGreying()
{
    //Handle state of checkQuicktest
    if (ui->checkQuickTest->isChecked()) {
        //Enable quick settings
        ui->Vca->setEnabled(true);
        ui->Vcs->setEnabled(true);
        ui->Vcg->setEnabled(true);
        ui->DeltaVa->setEnabled(true);
        ui->DeltaVs->setEnabled(true);
        ui->DeltaVg->setEnabled(true);
        ui->EmmVa->setEnabled(true);
        ui->EmmVs->setEnabled(true);
        ui->EmmVg->setEnabled(true);
        ui->EmmIa->setEnabled(true);

        //Disable sweep settings
        ui->VaStart->setEnabled(false);
        ui->VsStart->setEnabled(false);
        ui->VgStart->setEnabled(false);
        ui->VaEnd->setEnabled(false);
        ui->VsEnd->setEnabled(false);
        ui->VgEnd->setEnabled(false);
        ui->VaSteps->setEnabled(false);
        ui->VsSteps->setEnabled(false);
        ui->VgSteps->setEnabled(false);
    } else {
        //Disable quick settings
        ui->Vca->setEnabled(false);
        ui->Vcs->setEnabled(false);
        ui->Vcg->setEnabled(false);
        ui->DeltaVa->setEnabled(false);
        ui->DeltaVs->setEnabled(false);
        ui->DeltaVg->setEnabled(false);
        ui->EmmVa->setEnabled(false);
        ui->EmmVs->setEnabled(false);
        ui->EmmVg->setEnabled(false);
        ui->EmmIa->setEnabled(false);

        //Enable sweep settings
        ui->VaStart->setEnabled(true);
        ui->VgStart->setEnabled(true);
        ui->VaEnd->setEnabled(true);
        ui->VgEnd->setEnabled(true);
        ui->VaSteps->setEnabled(true);
        ui->VgSteps->setEnabled(true);
        if (!ui->checkVsEqVa->isChecked()) {
            ui->VsStart->setEnabled(true);
            ui->VsEnd->setEnabled(true);
            ui->VsSteps->setEnabled(true);
        }
        updateTubeGreying();
    }
}

//Tube data file management end
//-----------------------------


//------------------------------------------------------
// UI Slots
void MainWindow::on_actionExit_triggered()
{
    exit(0);
}

void MainWindow::on_actionCal_triggered()
{
    Cal_Dialog *w = new Cal_Dialog(this);
    w->show();
}

void MainWindow::on_actionOptions_triggered()
{
    options_dialog *w = new options_dialog(this);
    w->show();
}


void MainWindow::on_actionDebug_triggered()
{
    debug_dialog * w = new debug_dialog(this);
    w->show();
}

void MainWindow::on_AutoPath_editingFinished()
{
}

void MainWindow::on_actionSave_plot_triggered()
{
    QString dataFileName;
    if (ui->checkAutoNumber->isChecked() ) {
        dataFileName = ui->AutoPath->text()  ;
        dataFileName.append("/");
        dataFileName.append(ui->AutoFileName->text())  ;
        dataFileName.append("-");
        dataFileName.append(ui->AutoNumber->text());
        dataFileName.append(ui->AutoFileType->currentText());
    }
    else {
        dataFileName = QFileDialog::getSaveFileName(this,tr("Save plot file"),ui->AutoPath->text(),
                                                        tr("Formats (*.jpg; *.png; *.bmp; *.pdf)"));
    }
    if (dataFileName!="") {
        //qDebug() << "Save plot Filename:" << dataFileName;
        QList<QString> f =dataFileName.split(".");

        //save plot 1
        QString name;
        for (int i=0; i < plotTabs.length(); i++) {
            name.clear();
            QTextStream(&name) << f.first() << "_plot_" << (i+1) << "." << f.last();
            //(fileName, int  width = 0, int  height = 0, double  scale = 1.0, int  quality = -1  )
            if (f.last()=="jpg") plotTabs.at(i)->plotArea->saveJpg(name,0 , 0, 2.0,-1);
            else if (f.last()=="png") plotTabs.at(i)->plotArea->savePng(name, 0, 0, 2.0,-1);
            else if (f.last()=="pdf") plotTabs.at(i)->plotArea->savePdf(name);
            else if (f.last()=="bmp") plotTabs.at(i)->plotArea->saveBmp(name);
            else QMessageBox::information(NULL, "File Specification Error", "Only bmp, png, jpg and pdf format are supported.");
        }
    }
}


void MainWindow::on_actionSave_Data_triggered()
{
    static QString oldFileName;
    QString dataFileName;

    if (ui->checkAutoNumber->isChecked() ) {
        QString fn = ui->AutoFileName->text();
        dataFileName = ui->AutoPath->text()  ;
        dataFileName.append("/");
        dataFileName.append(ui->AutoFileName->text())  ;
        dataFileName.append("-");
        dataFileName.append(ui->AutoNumber->text());
        dataFileName.append(".txt");
    }
    else {
        dataFileName = QFileDialog::getSaveFileName(this,tr("Save Data file"),ui->AutoPath->text(),"Data log file(*.txt)");
    }
    if (dataFileName!="") {
        //qDebug() << "Data file name:" << dataFileName;
        int fs= dataFileName.lastIndexOf("/");
        fs= dataFileName.lastIndexOf(".");
        //Save the data
        QFile datafile(dataFileName);
        if (datafile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            //QTextStream out(&datafile);
            char m[256];
            datafile.write("Data 2\n");  //File format info

            datafile.write(ui->TubeSelector->currentText().toLatin1() );  //Type
            datafile.write("\n" );

            datafile.write(ui->TubeType->currentText().toLatin1() );  //Model
            datafile.write("\n" );
            if (ui->checkQuickTest->isChecked()) {
                sprintf(m,"Va Mid\t%d Delta Va\t%d\n",ui->Vca->text().toInt(), ui->DeltaVa->text().toInt());
                datafile.write(m);

                sprintf(m,"Vs/Va(b) Mid\t%d Delta Vs/Va(b)\t%d\n",ui->Vcs->text().toInt(), ui->DeltaVa->text().toInt());
                datafile.write(m);

                sprintf(m,"Vg Mid\t%f Delta Vg\t%d\n",ui->Vcg->text().toFloat(), ui->DeltaVg->text().toInt());
                datafile.write(m);
            }
            else {
                sprintf(m,"Va\t%d\t%d\t%d\n",(int)VaStart, (int)VaEnd, (int)VaSteps);
                datafile.write(m);

                sprintf(m,"Vs\t%d\t%d\t%d\n",(int)VsStart, (int)VsEnd, (int)VsSteps);
                datafile.write(m);

                sprintf(m,"Vg\t%d\t%d\t%d\n",(int)VgStart, (int)VgEnd, (int)VgSteps);
                datafile.write(m);
            }
            datafile.write("Point\tCurve\tIa\tIs\tVg\tVa\tVs\tVf\n");

            if (dataStore->length()>0) {
                //data
                results_t data_set;
                int curve=1, point=1, c=0;
                for(int i=0; i < dataStore->length(); i++) {
                    data_set = dataStore->at(i);
                    sprintf(m,"%d\t%d\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\n",
                            point,curve,data_set.Ia,data_set.Is,data_set.Vg,data_set.Va,data_set.Vs,data_set.Vf);
                    point++;
                    if (c==VaSteps) {
                        curve++; c=0; point=1;
                    } else c++;

                    datafile.write(m);
                }
                if (ui->checkQuickTest->isChecked()) {

                    QTextStream(&datafile) << QuickRes;
                }
                datafile.close();
            }
        } else
            QMessageBox::information(NULL, "File Error", "Problem creating data file. Check the filename and/or path");

    }
}

bool MainWindow::SetUpSweepParams() {

    bool ok;
    float Vf;
    //check the entries
    VaSteps = ui->VaSteps->text().toFloat(&ok);
    if (!ok){
        QMessageBox::information(NULL, "Entry Error", "Invalid entry for number of Va steps");
        return false;
    }
    VsSteps = ui->VsSteps->text().toFloat(&ok);
    if (!ok){
        QMessageBox::information(NULL, "Entry Error", "Invalid entry for number of Vs steps");
    return false;
    }

    VgSteps = ui->VgSteps->text().toFloat(&ok);
    if (!ok) {
        QMessageBox::information(NULL, "Entry Error", "Invalid entry for number of Vg steps");
    return false;
    }
    VaStart = ui->VaStart->text().toFloat(&ok);
    if (!ok) {
        QMessageBox::information(NULL, "Entry Error", "Invalid entry for Va start");
    return false;
    }
    VsStart = ui->VsStart->text().toFloat(&ok);
    if (!ok) {
        QMessageBox::information(NULL, "Entry Error", "Invalid entry for Vs start");
    return false;
    }
    VgStart = ui->VgStart->text().toFloat(&ok);
    if (!ok) {
        QMessageBox::information(NULL, "Entry Error", "Invalid entry for Vg start");
    return false;
    }
    VaEnd = ui->VaEnd->text().toFloat(&ok);
    if (!ok) {
        QMessageBox::information(NULL, "Entry Error", "Invalid entry for Va stop");
    return false;
    }
    VsEnd = ui->VsEnd->text().toFloat(&ok);
    if (!ok) {
        QMessageBox::information(NULL, "Entry Error", "Invalid entry for Vs stop");
    return false;
    }
    VgEnd = ui->VgEnd->text().toFloat(&ok);
    if (!ok){
        QMessageBox::information(NULL, "Entry Error", "Invalid entry for Vg stop");
    return false;
    }
    //check steps range
    if (VaSteps<0 || VaSteps>30) {
        QMessageBox::information(NULL, "Entry Error", "Va steps must be between 0 and 30");
    }
    //check steps range
    if (VsSteps<0 || VsSteps>30) {
        QMessageBox::information(NULL, "Entry Error", "Vs steps must be between 0 and 30");
    }
    //check steps range
    if (VgSteps<0 || VgSteps>30) {
        QMessageBox::information(NULL, "Entry Error", "Vg steps must be between 0 and 30");
    }
    //check Va, Vs, Vg range
    QString msg;
    QTextStream(&msg) << "Va start is " << VaStart << " but must be between " << VMIN << " and " << calData.VaMax;
    if (VaStart<VMIN || VaStart> calData.VaMax) {
        QMessageBox::information(NULL, "Entry Error", msg);
        return false;
    }
    msg.clear();
    QTextStream(&msg) << "Va End is " << VaEnd << " but must be between " << VaStart << " and " << calData.VaMax;
    if (VaEnd<VaStart || VaEnd> calData.VaMax) {
        QMessageBox::information(NULL, "Entry Error", msg);
        return false;
    }

    msg.clear();
    QTextStream(&msg) << "Vs start must be between " << VMIN << " and " << calData.VaMax;
    if (VsStart<VMIN || VsStart> calData.VaMax) {
        QMessageBox::information(NULL, "Entry Error ", msg);
        return false;
    }
    msg.clear();
    QTextStream(&msg) << "Vs End is " << VsEnd << " but must be between " << VsStart << " and " << calData.VaMax;
    if (VsEnd<VsStart || VsEnd> calData.VaMax) {
        QMessageBox::information(NULL, "Entry Error ", msg);
        return false;
    }
    //Check Vg range
    msg.clear();
    QTextStream(&msg) << "Vg start is " << VgStart << " but must be between 0 and " << (-calData.VgMax+VGLIM);
    if (VgStart>0 || VgStart < (-calData.VgMax+VGLIM)) {
        QMessageBox::information(NULL, "Entry Error", msg);
        return false;
    }
    msg.clear();
    QTextStream(&msg) << "Vg end is " << VgEnd << " but must be between 0 and " << (-calData.VgMax+VGLIM);
    if (VgEnd>0 || VgEnd < (-calData.VgMax+VGLIM)) {
        QMessageBox::information(NULL, "Entry Error", msg);
        return false;
    }
    //End > Start?
    if (VsEnd < VsStart ) {
        QMessageBox::information(NULL, "Entry Error", "Vs end must be greater than start");
        return false;
    }
    if (VsEnd < VsStart) {
        QMessageBox::information(NULL, "Entry Error", "Vs end must be greater than start");
        return false;
    }
    if (VgEnd > VgStart ) {
        QMessageBox::information(NULL, "Entry Error", "Vg end must be less than start");
        return false;
    }
    //Heater
    Vf = ui->VhVal->text().toFloat(&ok);
    if (!ok) {
        QMessageBox::information(NULL, "Entry Error", "Heater voltage entry is not valid.");
        return false;
    }
    if (Vf<0 or Vf>18) {
        QMessageBox::information(NULL, "Entry Error", "Heater voltage range must be  between 0 and 18.");
        return false;
    }
    return true;
}

void MainWindow::on_Start_clicked()
{
    RequestOperation((Operation_t) Start);
}
void MainWindow::CreateTestVectors()
{
    //create test vectors
    sweepList->clear();
    power = 0;
    tubeData.powerLim = ui->PowLim->text().toFloat();
    VaNow = ui->VaStart->text().toFloat();
    VsNow = ui->VsStart->text().toFloat();
    VgNow = ui->VgStart->text().toFloat();
    VaStart = ui->VaStart->text().toFloat();
    VsStart = ui->VsStart->text().toFloat();
    VgStart = ui->VgStart->text().toFloat();
    VaEnd = ui->VaEnd->text().toFloat();
    VsEnd = ui->VsEnd->text().toFloat();
    VgEnd = ui->VgEnd->text().toFloat();
    if (ui->checkQuickTest->isChecked()){
        if ((tubeData.Model==TRIODE) || (tubeData.Model==DUAL_TRIODE)) {
            ui->Vcs->setText( ui->Vca->text() );
        }
        test_vector.Va = ui->Vca->text().toFloat()*(1 - ui->DeltaVa->text().toFloat()/200);
        test_vector.Vs = ui->Vcs->text().toFloat();
        test_vector.Vg = ui->Vcg->text().toFloat();
        sweepList->append(test_vector);// Va

        test_vector.Va = ui->Vca->text().toFloat()*(1 + ui->DeltaVa->text().toFloat()/200);
        sweepList->append(test_vector); //Va

        test_vector.Va = ui->Vca->text().toFloat();
        test_vector.Vs = ui->Vcs->text().toFloat()*(1 - ui->DeltaVs->text().toFloat()/200);
        sweepList->append(test_vector);//Vs

        test_vector.Vs = ui->Vcs->text().toFloat()*(1 + ui->DeltaVs->text().toFloat()/200);
        sweepList->append(test_vector);//Vs

        test_vector.Vs = ui->Vcs->text().toFloat();
        test_vector.Vg = ui->Vcg->text().toFloat()*(1 - ui->DeltaVg->text().toFloat()/200);
        sweepList->append(test_vector);//Vg

        test_vector.Vg = ui->Vcg->text().toFloat()*(1 + ui->DeltaVg->text().toFloat()/200);
        sweepList->append(test_vector);//Vg

        //Emmission
        test_vector.Va = ui->EmmVa->text().toFloat()*(1 - ui->DeltaVa->text().toFloat()/200);
        test_vector.Vs = ui->EmmVs->text().toFloat();
        test_vector.Vg = ui->EmmVg->text().toFloat();
        sweepList->append(test_vector); //Emmision Vector

    }
    else
    {
        if (ui->checkVsEqVa->isChecked())
        {
            //make vs==Va
            for (int g = 0; g <= VgSteps; g ++)
            {
                test_vector.Vg = VgNow;
                VaNow = ui->VaStart->text().toFloat();
                if (VgSteps>0) VgNow += (VgEnd - VgStart)/(float)VgSteps;
                for (int a = 0; a <= VaSteps; a ++)
                {
                    test_vector.Vs = test_vector.Va = VaNow;
                    sweepList->append(test_vector);
                    if (VaSteps>0) VaNow += (VaEnd - VaStart)/(float)VaSteps;
                }
            }

        }
        else
        {
            //if using the pentode models, do a triode connected sweep first (for parameter estimation)
            //make vs==Va for a triode sweep
            if (ui->TubeType->currentText()!=NONE) { //skip if not modelling
            VgNow = ui->VgStart->text().toFloat();
            if (tubeData.Model==DERK_P || tubeData.Model==DERK_B || tubeData.Model==DERKE_P || tubeData.Model==DERKE_B)
                for (int g = 0; g <= VgSteps; g ++)
                {
                    test_vector.Vg = VgNow;
                    if (VgSteps>=1) VgNow += (VgEnd - VgStart)/(float)VgSteps;
                    VaNow = ui->VaStart->text().toFloat();
                    for (int a = 0; a <= VaSteps; a ++)
                    {
                        test_vector.Vs = test_vector.Va = VaNow;
                        sweepList->append(test_vector);
                        if (VaSteps>0) {
                            VaNow += (VaEnd - VaStart)/(float)VaSteps;
                            //VaNow = VaStart + (VaEnd-VaStart)*(pow(2,a/VaSteps)-1);
                        }
                    }
                }
            }
            //normal exhaustive sweep
            VsNow = ui->VsStart->text().toFloat();
            for (int s = 0; s <= VsSteps; s++)
            {
                if (VsNow>400) VsNow=400;
                test_vector.Vs = VsNow;
                if (VgSteps>=1) VsNow += (VsEnd - VsStart)/(float)VsSteps;
                VgNow = ui->VgStart->text().toFloat();
                for (int g = 0; g <= VgSteps; g++)
                {
                    test_vector.Vg = VgNow;
                    if (VgSteps>=1) VgNow += (VgEnd - VgStart)/(float)VgSteps;
                    VaNow = ui->VaStart->text().toFloat();
                    for (int a = 0; a <= VaSteps; a++)
                    {
                        if (VaNow>400) VaNow=400;
                        test_vector.Va = VaNow;
                        sweepList->append(test_vector);
                        if (VaSteps>=1) VaNow += (VaEnd - VaStart)/(float)VaSteps;
                    }
                }
            }
        }
    }

}

void MainWindow::on_VsStart_editingFinished() {
}

void MainWindow::on_Stop_clicked()
{
    RequestOperation((Operation_t) Stop);
}


/*
void MainWindow::on_plotDockWidget_dockLocationChanged(const Qt::DockWidgetArea &area)
{
//    ui->statusBar->showMessage("Dock Moved");
}


void MainWindow::on_plotDockWidget_topLevelChanged(bool topLevel)
{
    QRect winrect = QMainWindow::geometry();
    if (topLevel) {
        QMainWindow::setFixedSize(660,560);
    }
    else {
        QMainWindow::setFixedSize(1300,575);
    }
}
*/

void MainWindow::on_actionSave_Spice_Model_triggered()
{
    optimizer->Save_Spice_Model(this, ui->TubeSelector->currentText().toLatin1()) ;
}


//read a reference data set
void MainWindow::on_actionRead_Data_triggered()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Data File"),QDir::homePath(), tr("Data Files (*.txt)"));
    QFile datafile(fileName);
    if (datafile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        refStore->clear();
        QTextStream in(&datafile);
        QString line = in.readLine();
        bool valid = (line == "Data 2");

        int i = ui->TubeSelector->findText(in.readLine()); //name
        if (i !=-1 ) ui->TubeSelector->setCurrentIndex(i); else valid =false;

        i= ui->TubeType->findText(in.readLine()); //set model
        if ( i!=-1 ) ui->TubeType->setCurrentIndex(i); else valid =false;

        if (valid) {
            while ( !in.atEnd() && valid) {
                line = in.readLine();
                //Skip comments to get step and range info
                //qDebug() << "Read Data Set" << line;
                QStringList list = line.split('\t');
                if (line.at(0).toLatin1()==';') {
                    //comment
                }
                else if (list.at(0)=="Va") {
                    if (list.length()==4) {
                        VaStart = list.at(1).toFloat();
                        VsEnd = list.at(2).toFloat();
                        VaStep = VaSteps = list.at(3).toFloat();
                    } else valid=false;
                }
                else if (list.at(0)=="Vs") {
                    if (list.length()==4) {
                        VsStart = list.at(1).toFloat();
                        VsEnd = list.at(2).toFloat();
                        VsStep = VsSteps = list.at(3).toFloat();
                    } else valid=false;
                }
                else if (list.at(0)=="Vg") {
                    if (list.length()==4) {
                        VgStart = list.at(1).toFloat();
                        VgEnd = list.at(2).toFloat();
                        VgStep = VgSteps = list.at(3).toFloat();
                    } else valid=false;
                }
                else if (list.at(0)=="Point") {
                }
                else {
                    results.Ia = list.at(2).toFloat();
                    results.Is = list.at(3).toFloat();
                    results.Vg = list.at(4).toFloat();
                    results.Va = list.at(5).toFloat();
                    results.Vs = list.at(6).toFloat();
                    results.Vf = list.at(7).toFloat();
                    results.IaMod=0;
                    results.IsMod=0;
                    results.gm_a=0;
                    results.gm_b=0;
                    results.ra_a=0;
                    results.ra_b=0;
                    results.mu_a=0;
                    results.mu_b=0;
                    refStore->append(results);
                }
            }
        }
        datafile.close();
        if (valid)  {
            ui->statusBar->showMessage("Sucessfully read data file");
            optimizer->Optimize(refStore, ui->TubeType->currentIndex(), 1, VaSteps, VsSteps, VgSteps);
            //ui->PlotArea->clearGraphs();
            updateLcdsWithModel();
            RePlot(refStore);
        } else ui->statusBar->showMessage("Invalid data file");

    }
}

void MainWindow::on_checkVsEqVa_clicked()
{
    if(ui->checkVsEqVa->isChecked()) {
        ui->VsStart->setDisabled(true);
        ui->VsEnd->setDisabled(true);
        ui->VsSteps->setDisabled(true);
    }
    else {
        ui->VsStart->setEnabled(true);
        ui->VsEnd->setEnabled(true);
        ui->VsSteps->setEnabled(true);
    }
}

void MainWindow::on_checkQuickTest_clicked()
{
    updateSweepGreying();
}

/*------------------------------------------
 * Tab Management
 *------------------------------------------*/
void MainWindow::on_Tabs_currentChanged(int index)
{

    if (ignoreIndexChange==false) {
        if (index==ui->Tabs->count()-1  && index>0) {
            PlotTabWidget * plotTab  = new PlotTabWidget();
            ignoreIndexChange=true;
            ui->Tabs->insertTab(index,plotTab,QString::number(index));
            plotTabs.append(plotTab);
            ui->Tabs->setCurrentIndex(index);
            RePlot(activeStore);
            connect(plotTab, &PlotTabWidget::penChanged, this, &MainWindow::PenUpdate);
        }
    }
    else {
        ignoreIndexChange =  false;
    }
}

void MainWindow::on_Tabs_tabCloseRequested(int index)
{
    if (index<(ui->Tabs->count()-1)  && index>1) { //prevent the default plot tab from being closed
        ignoreIndexChange=true;
        disconnect(plotTabs.at(index-1), &PlotTabWidget::penChanged, this, &MainWindow::SaveCalFile);
        int c = ui->Tabs->count();
        delete plotTabs.at(index-1);
        plotTabs.removeAt(index-1);
        //ui->Tabs->removeTab(index);
        ui->Tabs->setCurrentIndex(index-1);
        //while (c==ui->Tabs->count()); //wait for tab to be removed
        for (int i=2 ; i < ui->Tabs->count()-1; i++) {
            ui->Tabs->setTabText(i,QString::number(i));
        }
        ui->Tabs->setTabText(ui->Tabs->count()-1,"New");
        ignoreIndexChange=false;
    }
}

void MainWindow::PenUpdate() {
    RePlot(activeStore);
    SaveCalFile();
}



void MainWindow::on_Browse_clicked()
{
    QString path = QFileDialog::getExistingDirectory(this, tr("Open Directory"),
                                                     QDir::homePath(),
                                                     QFileDialog::ShowDirsOnly
                                                     | QFileDialog::DontResolveSymlinks);
    if (path.length()>0) ui->AutoPath->setText(path);

}
