#ifndef DR_OPTIMIZE_H
#define DR_OPTIMIZE_H

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
#include <QChar>

class dr_optimize
{
public:
    dr_optimize();
    ~dr_optimize();
    void Optimize(QList<results_t> * dataSet, int model, int Weight, int, int, int);
    void Save_Spice_Model(QWidget *widget, QByteArray);
    float RaCalc(float Va, float Vs, float Vg);
    float GmCalc(float Va, float Vs, float Vg);
    float RaCalcB(float Va, float Vs, float Vg);
    float GmCalcB(float Va, float Vs, float Vg);
    float Rg2Calc(float Va, float Vs, float Vg);


private:
    struct muPair_t {
        float x, y;
    };

    struct IaParams_t {
        float p[14]; //Temporary Initial values
        float o[14]; //Optimized parameters
        int penfn; //tube type
        int f; //index of first parameter to optimize
        int n; //number of parameters
        double (* errFun)(const gsl_vector *x, void * param);
        bool section_b;
        int VaSteps;
        int VsSteps;
        int VgSteps;
        QList<results_t> * data;
    };
    IaParams_t IaParams;
    IaParams_t IbParams;

    QList<QString> * paramStrings;

    typedef  float (*IaCalc_t)(IaParams_t *, float, float, float) ;
    IaCalc_t iaFunc[10] = {0, &DiIa, &DiIa, &TriIa, &TriIa, &KpIa, &DpIa, &DBIa, &DEPIa, &DEBIa};


    void OptAll(QByteArray name);
    int TriodeInit();
    void EstimateKG2();
    void EstimateAP();
    void EstimateAB();
    void EstimateABE();
    void EstimateVW();
    void findLinear( int f, int l, float (*)(IaParams_t *, float, float, float) );

    static double DiErr(const gsl_vector *, void *);
    static double TriErr(const gsl_vector *, void *);
    static double KpAErr(const gsl_vector *, void *);
    static double KpSErr(const gsl_vector *, void *);
    static double DpErr(const gsl_vector *, void *);
    static double DEPErr(const gsl_vector *, void *);
    static double DBErr(const gsl_vector *, void *);
    static double DEBErr(const gsl_vector *, void *);

    static float DiIa(IaParams_t * p, float Va, float Vs,float Vg);
    static float TriIa(IaParams_t * p, float Va, float Vs,float Vg);
    static float KpIa(IaParams_t * p, float Va, float Vs, float Vg);
    static float KpIs(IaParams_t * p, float Va, float Vs, float Vg);
    static float DpIa(IaParams_t * p, float Va, float Vs, float Vg);
    static float DpIs(IaParams_t * p, float Va, float Vs, float Vg);
    static float DEPIa(IaParams_t * p, float Va, float Vs, float Vg);
    static float DEPIs(IaParams_t * p, float Va, float Vs, float Vg);
    static float DBIa(IaParams_t * p, float Va, float Vs, float Vg);
    static float DBIs(IaParams_t * p, float Va, float Vs, float Vg);
    static float DEBIa(IaParams_t * p, float Va, float Vs, float Vg);
    static float DEBIs(IaParams_t * p, float Va, float Vs, float Vg);

};

#endif // DR_OPTIMIZE_H
