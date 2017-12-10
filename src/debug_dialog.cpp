#include "debug_dialog.h"
#include "ui_debug_dialog.h"
#include <QMessageBox>
//#include "../3rdparty/qextserialport-1.2rc/src/qextserialport.h"
//#include "../3rdparty/qextserialport-1.2rc/src/qextserialenumerator.h"
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QDebug>


debug_dialog::debug_dialog(MainWindow *parent) :
    QDialog(parent),
    ui(new Ui::debug_dialog)
{
    ui->setupUi(this);
    mw = parent;
    UpdateComSel();
    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(UpdateVals()));
    timer->start(500);
}


debug_dialog::~debug_dialog()
{
    timer->stop();
    delete timer;
    delete ui;
}
void debug_dialog::UpdateVals()
{
    QString m;
    ui->TxVal->setText(mw->TxString);
    if(mw->echoString.length()>0) ui->RxVal->setText(mw->echoString);
    if(mw->statusString.length()>0) ui->StatusVal->setText(mw->statusString.mid(0,2));
    //Set command targets
    m.setNum((int)mw->VaNow,10);
    ui->VaTxVal->setText(m);
    m.setNum((int)mw->VsNow,10);
    ui->VsTxVal->setText(m);
    m.setNum((int)mw->VgNow,10);
    ui->VgTxVal->setText(m);
    m.setNum(mw->VfNow,'f',1);
    ui->VhTxVal->setText(m);

    m.setNum(mw->VaADC,10);
    ui->VaTxDecVal->setText(m);
    m.setNum(mw->VsADC,10);
    ui->VsTxDecVal->setText(m);
    m.setNum(mw->VgADC,10);
    ui->VgTxDecVal->setText(m);
    m.setNum(mw->VfADC,10);
    ui->VhTxDecVal->setText(m);

    m.setNum(mw->VaADC,16);
    ui->VaTxHex->setText(m);
    m.setNum(mw->VsADC,16);
    ui->VsTxHex->setText(m);
    m.setNum(mw->VgADC,16);
    ui->VgTxHex->setText(m);
    m.setNum(mw->VfADC,16);
    ui->VhTxHex->setText(m);
    //Update Adc readings
    mw->GetReal();
    m.setNum(mw->adc_data.Ia,16);
    ui->IaHexVal->setText(m);
    m.setNum(mw->adc_data.Ia_raw,16);
    ui->IacHexVal->setText(m);
    m.setNum(mw->adc_data.Is,16);
    ui->IsHexVal->setText(m);
    m.setNum(mw->adc_data.Is_raw,16);
    ui->IscHexVal->setText(m);
    m.setNum(mw->adc_data.Va,16);
    ui->VaHexVal->setText(m);
    m.setNum(mw->adc_data.Vs,16);
    ui->VsHexVal->setText(m);
    m.setNum(mw->adc_data.Vsu,16);
    ui->VsuHexVal->setText(m);
    m.setNum(mw->adc_data.Vn,16);
    ui->VnHexVal->setText(m);
    //Decimal
    m.setNum(mw->adc_data.Ia,10);
    ui->IaDecVal->setText(m);
    m.setNum(mw->adc_data.Ia_raw,10);
    ui->IacDecVal->setText(m);
    m.setNum(mw->adc_data.Is,10);
    ui->IsDecVal->setText(m);
    m.setNum(mw->adc_data.Is_raw,10);
    ui->IscDecVal->setText(m);
    m.setNum(mw->adc_data.Va,10);
    ui->VaDecVal->setText(m);
    m.setNum(mw->adc_data.Vs,10);
    ui->VsDecVal->setText(m);
    m.setNum(mw->adc_data.Vsu,10);
    ui->VsuDecVal->setText(m);
    m.setNum(mw->adc_data.Vn,10);
    ui->VnDecVal->setText(m);
    //Real
    m.setNum( mw->adc_real.Ia,'f',2);
    ui->IaVal->setText(m);
    m.setNum( mw->adc_real.Ia_raw,'f',2);
    ui->IacVal->setText(m);
    m.setNum( mw->adc_real.Is,'f',2);
    ui->IsVal->setText(m);
    m.setNum( mw->adc_real.Is_raw,'f',2);
    ui->IscVal->setText(m);
    m.setNum( mw->adc_real.Va,'f',2);
    ui->VaVal->setText(m);
    m.setNum( mw->adc_real.Vs,'f',2);
    ui->VsVal->setText(m);
    m.setNum( mw->adc_real.Vsu,'f',2);
    ui->VsuVal->setText(m);
    m.setNum( mw->adc_real.Vn,'f',2);
    ui->VnVal->setText(m);
    // Gains
    m.setNum( mw->adc_data.Gain_a,10);
    ui->IaGainHex->setText(m);
    m.setNum( (int)mw->adc_data.Gain_s,10);
    ui->IsGainHex->setText(m);
    m.setNum( mw->adc_real.Vg,'f',2);
    ui->VgVal->setText(m);
}

void debug_dialog::UpdateComSel()
{
    // Scrub current List
    openPortEn = false;
    ui->ComSel->clear();

    // Get a list of serial ports
    QList<QSerialPortInfo> serPortInfo = QSerialPortInfo::availablePorts();
    int portEntries = 0;
    int portNonEntries = 0;
    int firstEntry = -1;
    int firstNonEntry = -1;
    bool currentSerialPort = false;
    foreach (QSerialPortInfo port, serPortInfo)
    {
        if (!port.portName().isEmpty())
        {
            ui->ComSel->addItem(port.portName());
            qDebug() << "UpdateComSel(): serPortInfo:" << port.portName() << " comport:" << mw->comport;
            if (port.portName().contains(mw->comport))
            {
                ui->ComSel->setCurrentIndex(portNonEntries);
                currentSerialPort = true;
            }
            if (firstEntry == -1)
            {
                firstEntry = portEntries;
                firstNonEntry = portNonEntries;
            }
            portNonEntries++;
        }
        portEntries++;
    }

    // If the requested serial port is not found then select the first entry in the list (if available),
    // then open it, and update the calibration file with the new serial port name.
    if (!currentSerialPort && firstEntry != -1)
    {
        QString comport = serPortInfo.at(firstEntry).portName();
        qDebug() << "UpdateComSel(): Selecting the default serial port:" << comport;
        ui->ComSel->setCurrentIndex(firstNonEntry);
        mw->OpenComPort(&comport, true);
    }

    // If the list of serial ports is empty then ensure the serial port is closed
    if (!portNonEntries) mw->CloseComPort();

    openPortEn = true;
}


void debug_dialog::on_ComSel_currentIndexChanged(const QString &arg1)
{
    if (openPortEn) mw->OpenComPort(&arg1, true);
}

void debug_dialog::on_OK_clicked()
{
    close();
}

void debug_dialog::on_Ping_clicked()
{
    // Button is actually "Read ADC"
    mw->RequestOperation((Operation_t) ReadADC);
}


void debug_dialog::on_pushPing_clicked()
{
    mw->RequestOperation((Operation_t) Ping);
}
