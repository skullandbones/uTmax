#ifndef TYPEDEFS_H
#define TYPEDEFS_H
#include <QString>
#include <QList>
#include <QPen>

#define NONE "Do Not Model"
#define DIODE "Koren Diode"
#define DUAL_DIODE "Koren Dual Diode"
#define TRIODE "Koren Triode"
#define DUAL_TRIODE "Koren Dual Triode"
#define KOREN_P "Koren Pentode"
#define DERK_P "Derk Pentode"
#define DERK_B "Derk Pent+SE"
#define DERKE_P "DerkE BTet"
#define DERKE_B "DerkE BTet+SE"

struct results_t
{
    float Ia;
    float Is;
    float Va;
    float Vs;
    float Vg;
    float Vf;
    float IaMod;
    float IsMod;
    float gm_a;
    float gm_b;
    float ra_a;
    float ra_b;
    float mu_a;
    float mu_b;
};

struct plotInfo_t
{
    int VaSteps;
    int VsSteps;
    int VgSteps;
    float VaStart;
    float VaEnd;
    float VsStart;
    float VsEnd;
    float VgStart;
    float VgEnd;
    QList<results_t> *dataSet;
    QString tube;
    QString type;
    QList<QPen> *penList;
    bool penChange;
};

enum Operation_t
{
    Stop,
    Probe,
    Ping,
    ReadADC,
    Start
};

// Temporarily global
enum RxState_t
{
    RxIdle,
    RxEchoed,
    RxEchoError,
    RxResponse,
    RxComplete
};

// Temporarily global
enum TxState_t
{
    TxIdle,
    TxLoaded,
    TxSending,
    TxRxing,
    TxComplete
};

// Temporarily global
struct CommandResponse_t
{
    QByteArray Command;
    int txPos;
    TxState_t txState;
    int ExpectedRspLen;
    QByteArray Response;
    int rxPos;
    RxState_t rxState;
    int timeout;
};

#endif // TYPEDEFS_H
