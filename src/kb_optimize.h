#ifndef KB_OPTIMIZE_H
#define KB_OPTIMIZE_H
#include "typedefs.h"
#include <gsl/gsl_vector.h>
#include <gsl/gsl_multimin.h>
#include <QQueue>
#include <QFile>
#include <QString>
#include <QDebug>
#include <QFileDialog>
#include <QLineEdit>
#include <QtCore/qmath.h>



class kb_optimize
{
public:
    kb_optimize();
    ~kb_optimize();
    void SetUpModel(QList<results_t> * dataSet, int model, int Weight);
    void Optimize();
    void Save_Spice_Model(QWidget *widget, QByteArray);

private:

    //Ia
    struct IaParams_t {
        float mu,ex,kg1,kp,kvb,gp;
        float x[6]; //kg2 is not optimized
        int weight;
        int penfn;
        float max_vg;
        float vct;
        QList<results_t> * data;
    };
    IaParams_t IaParams;

    //Is
    struct IsParams_t {
        float I0, s, mup, mus, mug, ex;
        float x[6];
        int weight;
        int penfn;
        QList<results_t> * data;
    };
    IsParams_t IsParams;
    static double IaCalcErr(const gsl_vector *, void *);
    static double IsCalcErr(const gsl_vector *, void *);
    static float IaCalc(IaParams_t * param, float Va, float Vs, float Vg);
    static float IsCalc(IsParams_t * param, float Va, float Vs, float Vg);

};

#endif // KB_OPTIMIZE_H

