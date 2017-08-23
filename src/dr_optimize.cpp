#include "dr_optimize.h"
#include <QFile>
#include <QString>
#include <QDebug>
#include <QFileDialog>
#include <QtCore/qmath.h>

#define EK(Va,Vg,mu,kp,kvb) (Va/kp * log( 1 + exp( kp*(1/mu + Vg/sqrt(kvb +Va*Va) ) ) ) )

#define NONE 0
#define DIODE 1
#define DUAL_DIODE 2
#define TRIODE 3
#define DUAL_TRIODE 4
#define KOREN_P 5
#define DERK_P 6 // pentode + Ig2
#define DERK_B 7 //Pentode  + Ig2 +sec
#define DERKE_P 8 // BT + Ig2
#define DERKE_B 9 //BT +sec

#define KP 0
#define EX 1
#define KG1 2
#define MU 3
#define KVB 4
#define KG2 5
#define AP 6
#define ALPHA_S 7
#define BETA 8
#define LAMDA 9
#define S_ 10
#define NU 11
#define OMEGA 12
#define ASUBP 13

#define SIZE (5e-3)

dr_optimize::dr_optimize()
{
    paramStrings = new QList<QString>;
    paramStrings->append(" kp");
    paramStrings->append(" x");
    paramStrings->append(" kg1");
    paramStrings->append(" μ");
    paramStrings->append(" kvb");
    paramStrings->append(" kg2");
    paramStrings->append(" A");
    paramStrings->append(" αs");
    paramStrings->append(" β");
    paramStrings->append(" Lamda");
    paramStrings->append(" S");
    paramStrings->append(" Nu");
    paramStrings->append(" Omega");
    paramStrings->append(" αp");
}

dr_optimize::~dr_optimize()
{
    delete paramStrings;
}

//=========== SPICE model optimization =================
//fit a smooth curve through the given y vector
//0=Di 1=Kt 2=Kp 3=Dp 4=Dpb 5=De-p 6=De-bt
void dr_optimize::Optimize(QList<results_t> * dataSet, int Penfun, int Weight, int VaSteps_, int VsSteps_, int VgSteps_) {
    if (dataSet->length()<5) return;
    IbParams.penfn = IaParams.penfn = Penfun;
    IbParams.data = IaParams.data = dataSet;
    //clear parameters
    for (int i = 0; i < 13; i++) {
        IbParams.p[i] = IaParams.p[i] = IbParams.o[i] = IaParams.o[i]=0;
    }
    IbParams.VaSteps =IaParams.VaSteps =  VaSteps_;
    IbParams.VsSteps =IaParams.VsSteps =  VsSteps_;
    IbParams.VgSteps =IaParams.VgSteps =  VgSteps_;
    int n = IaParams.data->length();
    //Ia Params
    switch (Penfun) {
        case DIODE:
        {
             //Diode
            //data length Sanity check - we only do the calculation  when we have the correct data
            if (n!=(VaSteps_+1)) return;
            IaParams.p[EX] = 1.4;
            IaParams.p[KP]=1;
            IaParams.p[KG1]=500;
            IaParams.f = 0;
            IaParams.n = EX+1;
            IaParams.errFun = & dr_optimize::DiErr;
            OptAll("Diode");
            findLinear( 0, n, &dr_optimize::DiIa );
            break;
        }
        case DUAL_DIODE:
        {
             //Diode
            if (n!=(VaSteps_+1)) return;
            //section B
            IaParams.p[EX] = 1.4;
            IaParams.p[KP]=1;
            IaParams.p[KG1]=500;
            IaParams.f = 0;
            IaParams.n = EX+1;
            IaParams.section_b=true;
            IaParams.errFun = & dr_optimize::DiErr;
            OptAll("Diode B");
            IbParams.o[EX] = IaParams.o[EX];
            IbParams.o[KP] = IaParams.o[KP];
            IbParams.o[KG1] = IaParams.o[KG1];
            //section A
            IaParams.p[EX] = 1.4;
            IaParams.p[KP]=1;
            IaParams.p[KG1]=500;
            IaParams.section_b=false;
            IaParams.errFun = & dr_optimize::DiErr;
            OptAll("Diode A");
            findLinear( 0, n, &dr_optimize::DiIa );
            break;
        }
        case TRIODE:
        {
            //Koren Triode
            //data length Sanity check - we only do the calculation when we have all the data
            if (n!=(VaSteps_+1)*(VgSteps_+1)) return;
            IaParams.section_b = false;
            IaParams.p[EX] = 1.5;
            IaParams.p[KG1]=1000;
            IaParams.p[KP]=600;
            IaParams.p[KVB]=3;
            IaParams.p[MU]=50;
            IaParams.section_b = false;
            OptAll("Triode(all)");
            findLinear( 0, n, &dr_optimize::TriIa );
            break;
        }
        case DUAL_TRIODE:
        {
            //Koren Triode
            if (n!=(VaSteps_+1)*(VgSteps_+1)) return;

            //Optimize Section B
            IaParams.p[EX] = 1.4;
            IaParams.p[KG1]=500;
            IaParams.p[KP]=500;
            IaParams.p[KVB]=1;
            IaParams.p[MU]=50;            
            IaParams.section_b = true;
            TriodeInit();
            IaParams.f = 0;
            IaParams.n = KVB+1;
            IaParams.errFun = & dr_optimize::TriErr;
            OptAll("KOREN(triB)");
            //Save results into B section
            IbParams.o[EX] =IaParams.o[EX];
            IbParams.o[KG1]= IaParams.o[KG1];
            IbParams.o[KP]= IaParams.o[KP];
            IbParams.o[KVB]= IaParams.o[KVB];
            IbParams.o[MU]= IaParams.o[MU];

            //section A
            IaParams.p[EX] = 1.4;
            IaParams.p[KG1]=500;
            IaParams.p[KP]=500;
            IaParams.p[KVB]=1;
            IaParams.p[MU]=50;
            IaParams.section_b = false;
            TriodeInit();
            IaParams.f = 0;
            IaParams.n = KVB+1;
            IaParams.errFun = &dr_optimize::TriErr;
            OptAll("KOREN(triA)");
            findLinear( 0, n, &dr_optimize::TriIa );
            break;
        }
        case KOREN_P:
        {
            //Koren Pentode
            //data length Sanity check - we only do the calculation  when we have the correct data
            if (n!=(VaSteps_+1)*(VgSteps_+1) + (VaSteps_+1)*(VsSteps_+1)*(VgSteps_+1)) return;

            IaParams.section_b = false;
            TriodeInit();
            IaParams.f = 0;
            IaParams.n = KVB+1;
            IaParams.errFun = &dr_optimize::TriErr;
            OptAll("KOREN(tri)");

            //IaParams.p[MU]=20;
            //IaParams.p[EX] = 1.4;
            //IaParams.p[KG1]=2000;
            //IaParams.p[KP]=250;
            //IaParams.p[KVB]=25;
            //IaParams.p[KG2]=5000;
            IaParams.f = 0;
            IaParams.n = KVB+1;
            IaParams.errFun = &dr_optimize::KpAErr;
            OptAll("KOREN(penA)");
            EstimateKG2();
            IaParams.f = KG2;
            IaParams.n = KG2+1;
            IaParams.errFun = &dr_optimize::KpSErr;
            OptAll("KOREN(penS)");
            findLinear( (IaParams.VaSteps+1)*(IaParams.VgSteps+1), IaParams.data->length(), &dr_optimize::KpIa );
            break;
        }
        case DERK_P:
        {
            //data length Sanity check - we only do the calculation  when we have the correct data
            if (n!=(VaSteps_+1)*(VgSteps_+1) + (VaSteps_+1)*(VsSteps_+1)*(VgSteps_+1)) return;
            IaParams.section_b = false;
            TriodeInit(); //get guesses for a triode
            IaParams.section_b=false;
            IaParams.f = 0;
            IaParams.n = KVB+1;
            IaParams.errFun = &dr_optimize::TriErr;
            OptAll("DERK_P(tri)");

            EstimateKG2();
            EstimateAP();
            EstimateAB();
            IaParams.f = 0;
            IaParams.n = BETA+1;
            IaParams.errFun = &dr_optimize::DpErr;
            OptAll("DERK_P(all)");
            findLinear( (IaParams.VaSteps+1)*(IaParams.VgSteps+1), IaParams.data->length(), &dr_optimize::DpIa );
            break;
        }
        case DERK_B:
        {
            //Derk Pentode with sec emmission
            //data length Sanity check - we only do the calculation  when we have the correct data
            if (n!=(VaSteps_+1)*(VgSteps_+1) + (VaSteps_+1)*(VsSteps_+1)*(VgSteps_+1)) return;
            IaParams.section_b = false;
            TriodeInit(); //get guesses for a triode
            //optimize triode params
            IaParams.f = 0;
            IaParams.n = KVB+1;
            IaParams.errFun = &dr_optimize::TriErr;
            OptAll("DERK_B(tri)");

            //all parameters
            EstimateKG2();
            EstimateAP();
            EstimateAB();

            IaParams.p[LAMDA]=IaParams.p[MU];
            EstimateVW();
            //IaParams.p[OMEGA]=80;
            //IaParams.p[NU]=1;
            IaParams.p[S_]=0.01;
            IaParams.p[ASUBP]=0.02;
            IaParams.f = 0;
            IaParams.n = BETA+1;
            IaParams.errFun = &dr_optimize::DBErr;
            OptAll("DERK_B(no se)");

            //just se params
            IaParams.f = LAMDA;
            IaParams.n = ASUBP+1;
            IaParams.errFun = &dr_optimize::DBErr;
            OptAll("DERK_B(se)");

            //all parameters
            IaParams.f = 0;
            IaParams.n = ASUBP+1;
            IaParams.errFun = &dr_optimize::DBErr;
            OptAll("DERK_B(all)");
            findLinear( (IaParams.VaSteps+1)*(IaParams.VgSteps+1), IaParams.data->length(), &dr_optimize::DBIa );
            break;
        }
        case DERKE_P:
        {
            //DerkE-P= BT
            //data length Sanity check - we only do the calculation  when we have the correct data
            if (n!=(VaSteps_+1)*(VgSteps_+1) + (VaSteps_+1)*(VsSteps_+1)*(VgSteps_+1)) return;

            IaParams.section_b=false;
            TriodeInit(); //get guesses for a triode A

            //optimize triode params
            IaParams.f = 0;
            IaParams.n = KVB+1;
            IaParams.errFun = &dr_optimize::TriErr;
            OptAll("DERKE_P(tri)");

            EstimateKG2();
            EstimateAP();
            EstimateABE();

            //optimize pentode params
            IaParams.f = KG2;
            IaParams.n = BETA+1;
            IaParams.errFun = &dr_optimize::DEPErr;
            OptAll("DERKE_P(pen)");

            //optimize all params
            IaParams.f = 0;
            IaParams.n = BETA+1;
            IaParams.errFun = &dr_optimize::DEPErr;
            OptAll("DERKE_P(all)");
            findLinear( (IaParams.VaSteps+1)*(IaParams.VgSteps+1), IaParams.data->length(), &dr_optimize::DEPIa );
            break;
        }
        case DERKE_B:
        {
            //DerkE-B BT + sec
            //data length Sanity check - we only do the calculation  when we have the correct data
            if (n!=(VaSteps_+1)*(VgSteps_+1) + (VaSteps_+1)*(VsSteps_+1)*(VgSteps_+1)) return;
            IaParams.section_b = false;
            TriodeInit(); //get guesses for a triode
            IaParams.section_b=false;
            //optimize triode params
            IaParams.f = 0;
            IaParams.n = KVB+1;
            IaParams.errFun = &dr_optimize::TriErr;
            OptAll("DERKE_B(Tri)");

            EstimateKG2();
            EstimateAP();
            EstimateABE();
            IaParams.p[LAMDA]=IaParams.p[MU];
            EstimateVW();
            //IaParams.p[OMEGA]=80;
            //IaParams.p[NU]=1;
            IaParams.p[S_]=0.01;
            IaParams.p[ASUBP]=0.02;
            IaParams.f = 0;
            IaParams.n = BETA+1;
            IaParams.errFun = &dr_optimize::DEBErr;
            OptAll("DERKE_B(no se)");

            //just se params
            IaParams.f = LAMDA;
            IaParams.n = ASUBP+1;
            IaParams.errFun = &dr_optimize::DEBErr;
            OptAll("DERKE_B(se)");

            //all parameters
            IaParams.f = 0;
            IaParams.n = ASUBP+1;
            IaParams.errFun = &dr_optimize::DEBErr;
            //OptAll("DERKE_B(all)");
            findLinear( (IaParams.VaSteps+1)*(IaParams.VgSteps+1), IaParams.data->length(), &dr_optimize::DEBIa );
            break;
        }
    }
}

//Calculates anode resistance in Kohms
float dr_optimize::RaCalc(float Va, float Vs, float Vg) {
    IaParams.section_b=false;
    float a = iaFunc[IaParams.penfn](&IaParams, Va, Vs, Vg);
    float b = iaFunc[IaParams.penfn](&IaParams, Va-0.1, Vs, Vg);
    return 0.1/(a-b);
}

//Calculates Gm in mS
float dr_optimize::GmCalc(float Va, float Vs, float Vg) {
    IbParams.section_b=true;
    float a = iaFunc[IaParams.penfn](&IaParams, Va, Vs, Vg);
    float b = iaFunc[IaParams.penfn](&IaParams, Va, Vs, Vg-0.1);
    return (a-b)/0.1;
}

//TBD: Calculates Rg2 in Kohms
float dr_optimize::Rg2Calc(float Va, float Vs, float Vg) {
    float a = iaFunc[IaParams.penfn](&IaParams, Va, Vs, Vg);
    float b = iaFunc[IaParams.penfn](&IaParams, Va, Vs-0.1, Vg);
    return 0.1/(a-b);
}

float dr_optimize::RaCalcB(float Va, float Vs, float Vg) {
    IaParams.section_b=false;
    IbParams.section_b=true;
    float a = iaFunc[IaParams.penfn](&IbParams, Va, Vs, Vg);
    float b = iaFunc[IaParams.penfn](&IbParams, Va-0.1, Vs, Vg);
    return 0.1/(a-b);
}

//Calculates Gm B in mS
float dr_optimize::GmCalcB(float Va, float Vs, float Vg) {
    IaParams.section_b=false;
    IbParams.section_b=true;
    float a = iaFunc[IaParams.penfn](&IbParams, Va, Vs, Vg);
    float b = iaFunc[IaParams.penfn](&IbParams, Va, Vs, Vg-0.1);
    return (a-b)/0.1;
}

void dr_optimize::findLinear(int first, int last, float (* iaFunc)(IaParams_t * p, float Va, float Vs, float Vg)) {

    //find Gm (Steepness) using model
    float a, b, c;
    results_t r;
    for (int i=first; i < last; i++) {
        r = IaParams.data->at(i);
        //ra
        IaParams.section_b=false;
        IbParams.section_b=true;
        a = iaFunc(&IaParams, IaParams.data->at(i).Va, IaParams.data->at(i).Vs, IaParams.data->at(i).Vg);
        b = iaFunc(&IaParams, IaParams.data->at(i).Va-0.1, IaParams.data->at(i).Vs, IaParams.data->at(i).Vg);
        if (a-b <= 1e-5) r.ra_a =0; else r.ra_a = 0.1/(a-b);
        if (!(IaParams.penfn == DUAL_DIODE || IaParams.penfn == DIODE )) {
            //gm
            c = iaFunc(&IaParams, IaParams.data->at(i).Va, IaParams.data->at(i).Vs, IaParams.data->at(i).Vg-0.1);
            r.gm_a = (a-c)/0.1;
            //mu
            r.mu_a =  r.ra_a * r.gm_a;
        }
        if (IaParams.penfn == DUAL_TRIODE || IaParams.penfn == DUAL_DIODE ) {
            a = iaFunc(&IbParams, IaParams.data->at(i).Vs, IaParams.data->at(i).Vs, IaParams.data->at(i).Vg);
            b = iaFunc(&IbParams, IaParams.data->at(i).Vs-0.1, IaParams.data->at(i).Vs, IaParams.data->at(i).Vg);
            if (a-b <= 1e-5) r.ra_b =0;  else r.ra_b = 0.1/(a-b);
            if (IaParams.penfn == DUAL_TRIODE ) {
                //gm
                c = iaFunc(&IbParams, IaParams.data->at(i).Vs, IaParams.data->at(i).Vs, IaParams.data->at(i).Vg-0.1);
                r.gm_b = (a-c)/0.1;
                //mu
                r.mu_b =  r.ra_b * r.gm_b;
            }
        }
        //Update data table
        IaParams.data->replace(i,r);
    }
}

void dr_optimize::EstimateKG2(){

    //Derk Reefman, Theory, sec 10.2.1

    int i,k=0;
    IaParams.p[KG2]=0;
    for (i=0; i< IaParams.data->length(); i++) {
        if (IaParams.data->at(i).Va >= 100) {
/*            IaParams.p[KG2] +=
                    1000*EK(IaParams.data->at(i).Vs,
                       IaParams.data->at(i).Vg,
                       IaParams.p[MU],
                       IaParams.p[KP],
                       IaParams.p[KVB])/IaParams.data->at(i).Is;*/
            if (IaParams.data->at(i).Ia !=-1) {
                k++;
                IaParams.p[KG2] += IaParams.p[KG1] * IaParams.data->at(i).Ia/IaParams.data->at(i).Is;
            }
        }
    }
    if (k>0) IaParams.p[KG2]/=k; else IaParams.p[KG2]=IaParams.p[KG1]*3;
    //IaParams.p[KG2]=IaParams.p[KG1]*3;
}


void dr_optimize::EstimateAP(){

    //Derk Reefman, Theory, sec 10.2.2
    //In the pentode section, take two points where only Va has changed and where Va is high
    int h = IaParams.VaSteps-1 + (IaParams.VaSteps+1) * (IaParams.VgSteps+1);
    while (IaParams.data->at(h).Ia==-1) h-=1;
    float Ga = (IaParams.data->at(h).Ia - IaParams.data->at(h-1).Ia)/1000/(IaParams.data->at(h).Va - IaParams.data->at(h-1).Va);
    IaParams.p[AP] = IaParams.p[KG1]* Ga / pow(EK(IaParams.data->at(h).Vs, IaParams.data->at(h).Vg, IaParams.p[MU],IaParams.p[KP],IaParams.p[KVB]),IaParams.p[EX]);
}

void dr_optimize::EstimateAB(){

    //Derk Reefman, Theory, sec 10.2.3
    //Build  a list of points where the Va is smallest
    muPair_t pair;
    QList<muPair_t> * estList = new QList<muPair_t>;
    //The two lowest values of Va are at point (0,1), (VaSteps, VSteps+1) ...
    //char m[256];
    //QFile datafile("AB_List.txt");
    //if (datafile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        //sprintf(m,"x\ty\n");
        //datafile.write(m);
        for (int i=(IaParams.VaSteps+1)*(IaParams.VgSteps+1); i< IaParams.data->length(); i+=1) {
            if (IaParams.data->at(i).Va < IaParams.data->at(1).Va+5) {
            pair.x = IaParams.data->at(i).Va;
            float Ipk = pow(EK(IaParams.data->at(i).Vs, IaParams.data->at(i).Vg, IaParams.p[MU],IaParams.p[KP],IaParams.p[KVB]),IaParams.p[EX]);
            pair.y = 1/(IaParams.p[KG2] * IaParams.data->at(i).Is/Ipk/1000 -1);
            estList->append(pair);
            //sprintf(m,"%f\t%f\n",pair.x,pair.y);
            //datafile.write(m);
        }
    }
    //datafile.close();
    //} else return;
    // find the best fit slope through these points
    float xm=0, ym=0;
    for (int i=0; i < estList->length();i++) {
        xm += estList->at(i).x;
        ym += estList->at(i).y;
    }
     xm /= estList->length();
     ym /= estList->length();
     float yd=0, xd=0;
     for (int i=0; i < estList->length();i++){
         xd += (estList->at(i).x-xm) * (estList->at(i).x-xm);
         yd += (estList->at(i).y-ym) * (estList->at(i).x-xm);
     }
     float a = yd/xd;
     float b = ym - a*xm;
     IaParams.p[ALPHA_S] =  1/b;
     IaParams.p[BETA] = a/b;
     delete estList;
}

void dr_optimize::EstimateABE() {

    //Derk Reefman, Theory, sec 10.2.4
    //Build  a list of points where the Va is smallest
    muPair_t pair;
    QList<muPair_t> * estList = new QList<muPair_t>;
    //The two lowest values of Va are at point (0,1), (VaSteps, VSteps+1) ...
    //char m[256];
    //QFile datafile("ABE_List.txt");
    float y;
    //if (datafile.open(QIODevice::WriteOnly | QIODevice::Text)) {
    //    sprintf(m,"x\ty\n");
    //    datafile.write(m);
        for (int i=(IaParams.VaSteps+1)*(IaParams.VgSteps+1); i< IaParams.data->length(); i+=1) {
            if ((IaParams.data->at(i).Va < IaParams.data->at(1).Va+5) && (IaParams.data->at(i).Ia!=-1))
            {
                pair.x = pow(IaParams.data->at(i).Va,1.5);
                float Ipk = pow(EK(IaParams.data->at(i).Vs, IaParams.data->at(i).Vg, IaParams.p[MU],IaParams.p[KP],IaParams.p[KVB]),IaParams.p[EX]);
                y = IaParams.p[KG2] * IaParams.data->at(i).Is/Ipk/1000 -1;
                y = y>0? y:0.1;
                pair.y = log(y);
                estList->append(pair);
                //sprintf(m,"%f\t%f\n",pair.x,pair.y);
                //datafile.write(m);
            }
        }
        //datafile.close();
    //} else return;
    // find the best fit slope through these points
    float xm=0, ym=0;
    for (int i=0; i < estList->length();i++) {
        xm += estList->at(i).x;
        ym += estList->at(i).y;
    }
     xm /= estList->length();
     ym /= estList->length();
     float yd=0, xd=0;
     for (int i=0; i < estList->length();i++){
         xd += (estList->at(i).x-xm) * (estList->at(i).x-xm);
         yd += (estList->at(i).y-ym) * (estList->at(i).x-xm);
    }
    float a = yd/xd;
    float b = ym - a*xm;
    IaParams.p[ALPHA_S] =  exp(b);
    //IaParams.p[ALPHA_S] =  2e-6;
    IaParams.p[BETA] = pow(-a,0.667)  ;
    //IaParams.p[BETA] = 0.025  ;
    delete estList;
}

void dr_optimize::EstimateVW(){

    muPair_t pair;
    QList<muPair_t> * estList = new QList<muPair_t>;
    //find 2nd derivitive of Ig2 over all samples
    int i=0;
    float h=0,l=0, Imax=0;
    for (int g=0; g < IaParams.VgSteps+1; g++) for (int s=0; s < IaParams.VsSteps+1; s++) for (int a=0; a < IaParams.VaSteps+1 ; a++) {
        i = a + (IaParams.VaSteps+1)*s + (IaParams.VsSteps+1)*(IaParams.VaSteps+1)*g + (IaParams.VgSteps+1)*(IaParams.VaSteps+1);  //start in pentode section
        if (a<1) pair.x = pair.y = 0; else {
            h = IaParams.data->at(i  ).Is - IaParams.data->at(i-1).Is;
            //l = IaParams.data->at(i-1).Is - IaParams.data->at(i-2).Is;
            pair.x = (IaParams.data->at(i).Va +IaParams.data->at(i-1).Va)/2; //Va at midpoint
            pair.y = h;
        }
        estList->append(pair);
        Imax = Imax<IaParams.data->at(i).Is ? IaParams.data->at(i).Is : Imax;
    }
    //find local maxima point in Ig2, if one
    Imax/=300;
    float Va=0,c=0, Vh, Vl;
    QList<muPair_t> * slopeList = new QList<muPair_t>;
    for (i=1; i < estList->length(); i++) {
        h = estList->at(i).y;
        l = estList->at(i-1).y;
        Vh = estList->at(i).x;
        Vl = estList->at(i-1).x;
        if ( (h < 0) && (l > 0)&& (Imax < fabs(h-l)) ) {
            c = fabs(l/(h-l)) * (Vh - Vl) + Vl;
            pair.x =  IaParams.data->at(i+(IaParams.VgSteps+1)*(IaParams.VaSteps+1)).Vg;
            pair.y = c - IaParams.data->at(i+(IaParams.VgSteps+1)*(IaParams.VaSteps+1)).Vs/IaParams.p[LAMDA];
            slopeList->append(pair);
        }
    }
    if (slopeList->length()<2) {IaParams.p[NU]= IaParams.p[OMEGA]=0;}
    else {
        // find the best fit slope through these points
        float xm=0, ym=0;
        for (int i=0; i < slopeList->length();i++) {
            xm += slopeList->at(i).x;
            ym += slopeList->at(i).y;
        }
         xm /= slopeList->length();
         ym /= slopeList->length();
         float yd=0, xd=0;
         for (int i=0; i < slopeList->length();i++){
             xd += (slopeList->at(i).x-xm) * (slopeList->at(i).x-xm);
             yd += (slopeList->at(i).y-ym) * (slopeList->at(i).x-xm);
        }
        h = yd/xd;
        l = ym - h*xm;
        IaParams.p[NU] = -h;
        IaParams.p[OMEGA] = -l;
        IaParams.p[NU]=5;
        IaParams.p[OMEGA]=50;
    }
    delete estList;
    delete slopeList;
}

int dr_optimize::TriodeInit()
{

    //Default estimates
    int n = (IaParams.VaSteps+1)*(IaParams.VgSteps+1);
    float a=0;
    //Initial estimates,
    //if a pentode, we only use the first Va=Vs triode sweep section consisting of (VaSteps+1)*(VgSteps+1) data points
    //find Max Va and Max Ia
    float IaMax = 0;
    int ln = (IaParams.VaSteps+1)*(IaParams.VgSteps+1);
    if (ln > IaParams.data->length()) return -1;
    for (int i =0; i < ln; i++) {
        if (IaMax < IaParams.data->at(i).Ia) IaMax = IaParams.data->at(i).Ia;
    }
    //Chose mu where Ia = 0.05 of max
    //find Indices where Ia crosses Imax/20
    int l =0, h=0;
    muPair_t pair;
    QList<muPair_t> * estList = new QList<muPair_t>;
    for (int ai=0; ai <= IaParams.VaSteps; ai++) {
        for (int gi=1; gi <= IaParams.VgSteps; gi++) {
            h = ai + (IaParams.VaSteps+1) * (gi-1);
            l = ai + (IaParams.VaSteps+1) * gi;
            float ia_l = IaParams.data->at(l).Ia;
            float ia_h = IaParams.data->at(h).Ia;
            if (IaParams.data->at(l).Ia < IaMax/20 ) {
                 if (IaMax/20 <= IaParams.data->at(h).Ia)
                {
                    //found a cross, estimate Vg and Va at IaMax/20 and add to list
                    float f = (IaMax/20 - IaParams.data->at(l).Ia)/(IaParams.data->at(h).Ia - IaParams.data->at(l).Ia); //fraction
                    pair.x = f*(IaParams.data->at(h).Vg - IaParams.data->at(l).Vg) + IaParams.data->at(l).Vg; //Interpolate Vg
                    pair.y = f*(IaParams.data->at(h).Va - IaParams.data->at(l).Va) + IaParams.data->at(l).Va; //Interpolate Va
                    estList->append(pair);
               }
            }
        }
    }
    //find average mu using pairs in the estList
    // ---------------------------------------
    a=0;
    for (int i = 0; i < estList->length()-1; i++ ) {
        a -= (estList->at(i+1).y - estList->at(i).y)/(estList->at(i+1).x - estList->at(i).x);
    }
    a /= (float)(estList->length()-1);
    if ( a>5 && a <=200) {
        IaParams.p[MU] =a;
    } else {
        IaParams.p[MU] =25;
        qDebug() << "TriodeInit: bad estimate for MU overridden";
    }
    estList->clear();

    //estimate kg1, ex
    // ---------------------------------------
    for(int i=0; i < n; i++) {
        //create a plot of ln(ia) vs ln(Va/mu +Vg)
        float x = IaParams.data->at(i).Va/IaParams.p[MU] + IaParams.data->at(i).Vg;
        if (x>0 && IaParams.data->at(i).Ia!=-1) { //check for suitable point
            pair.x = log(x);
            pair.y = log(IaParams.data->at(i).Ia/1000);
            estList->append(pair);
        }
    }
    //determine ex, kg1, kp
    // ---------------------------------------
    if (estList->length()!=0) {
        a=0;
        //determine ex from the average slope
        // ---------------------------------------
        for(int i=1; i < estList->length(); i++) {
            a += (estList->at(i).y-estList->at(i-1).y)/(estList->at(i).x-estList->at(i-1).x);
        }
        a /= estList->length()-1;
        if ( a>1.1 && a <=1.6) {
            IaParams.p[EX] =a;
        } else {
            qDebug() << "TriodeInit: bad estimate for 'x' overridden";
            IaParams.p[EX] =1.35;
        }

        //determine kg1 from the average y axis intercept
        // ---------------------------------------
        a=0 ;
        for(int i=0; i < estList->length(); i++) {
            a += exp(IaParams.p[EX] * estList->at(i).x - estList->at(i).y);
        }
        a /= estList->length();
        IaParams.p[KG1] = a;

        //estimate kp
        // ---------------------------------------
        estList->clear();
        for(int i=0; i < n; i++){
            if (IaParams.data->at(i).Vg!=0 && IaParams.data->at(i).Va>100 && IaParams.data->at(i).Ia!=-1) {
                pair.x = 1/IaParams.p[MU] + IaParams.data->at(i).Vg/IaParams.data->at(i).Va;
                pair.y = log(IaParams.data->at(i).Ia/1000 * IaParams.p[KG1]) /IaParams.p[EX];
                estList->append(pair);
            }
        }
        //find average kp from slopes
        a=0;
        for(int i=1; i < estList->length(); i++) {
            a += (estList->at(i).y - estList->at(i-1).y)/(estList->at(i).x - estList->at(i-1).x);
        }
        a /= (float)(estList->length()-1);
        IaParams.p[KP] = a;

        //estimate kvb
        // ---------------------------------------
        estList->clear();
        IaParams.p[KVB]=0;
        a=0;
        int k=0; 
        float kvb=0, kvb1,e;
        float e1;
        for(int i=0; i < n; i++) {
            kvb = 5000; //guess for KP
            kvb1 = 2500;
            if (IaParams.data->at(i).Ia!=-1) {
                e = pow(IaParams.data->at(i).Ia/1000, IaParams.p[EX]);
                do {
                    //EK(Va,Vg,mu,kp,kvb)
    //                e1 = EK(IaParams.data->at(i).Va, IaParams.data->at(i).Vg, IaParams.p[MU], IaParams.p[KP],kvb);
                    e1 = IaParams.data->at(i).Va/IaParams.p[KP] *
                        log( 1+
                            exp(IaParams.p[KP]*
                            (1/IaParams.p[MU] + IaParams.data->at(i).Vg/
                             sqrt(kvb + IaParams.data->at(i).Va*IaParams.data->at(i).Va) ))
                            );
                    if (e1>e) kvb = kvb + kvb1; else kvb = kvb - kvb1;
                    kvb1 /=2;
                } while (kvb1>1);
                k++;
                IaParams.p[KVB] += kvb1;
            }
        }
            /* if (IaParams.p[KP] *( 1/IaParams.p[MU] + IaParams.data->at(i).Vg/IaParams.data->at(i).Va) >2)
            {
                a = IaParams.data->at(i).Vg/(
                        pow(IaParams.data->at(i).Ia/1000*IaParams.p[KG1],1/IaParams.p[EX])
                        /IaParams.data->at(i).Va
                        - 1/IaParams.p[MU]
                    );
                k++;
                b =  a*a - IaParams.data->at(i).Va * IaParams.data->at(i).Va;
                IaParams.p[KVB] += b;
            }*/
        IaParams.p[KVB] /= n;
    }
    //done with estimate list
    if (estList !=0) delete estList;
}
/*-----------------------------------
  The optimizer setup procedure
 -----------------------------------*/
void dr_optimize::OptAll(QByteArray name)
{
    int n  = IaParams.data->length();
    int iter, status;
    int num_params = IaParams.n-IaParams.f;
    if  (n >= num_params) {
        //Minimize Ia errors
        int max_iter=1000;
        double size;
        //init starting point and unused parameters
        for (int i=0; i <= ASUBP; i++) IaParams.o[i] = IaParams.p[i];
        QString * buf = new QString;
        buf->append(name);
        buf->append(" starts:         ");
        for (int i=0; i < IaParams.n; i++) {
            buf->append(paramStrings->at(i));
            buf->append(":");
            buf->append( QString::number(IaParams.p[i]));
        }
        qDebug() << *buf;
        buf->clear();
        gsl_vector * x = gsl_vector_alloc(num_params);
        gsl_vector_set_all(x,1.0);
        //init step size
        gsl_vector * steps = gsl_vector_alloc(num_params);
        gsl_vector_set_all(steps, 0.1);
        gsl_multimin_function f;
        f.f = (double (* ) (const gsl_vector * x, void * params)) IaParams.errFun;
        f.n = num_params;
        f.params = (void *) & IaParams.p;
        const gsl_multimin_fminimizer_type  * T = gsl_multimin_fminimizer_nmsimplex;
        gsl_multimin_fminimizer  * solver = NULL;
        solver = gsl_multimin_fminimizer_alloc(T,num_params);
        gsl_multimin_fminimizer_set(solver, &f, x, steps);
        iter =0;
        do {
          iter++;
          status = gsl_multimin_fminimizer_iterate(solver);
          if (status) break;
          size = gsl_multimin_fminimizer_size(solver);
          status = gsl_multimin_test_size(size, SIZE);
        } while (status == GSL_CONTINUE && iter < max_iter);
//        if (status == GSL_SUCCESS)
        for (int i=IaParams.f; i< IaParams.n; i++) IaParams.p[i] = IaParams.o[i]=  IaParams.p[i] * fabs(gsl_vector_get(solver->x ,i-IaParams.f));
        //report
        buf->append(name);
        buf->append(" ends @ iter ");
        buf->append( QString::number(iter));
        buf->append(":");
        for (int i=0; i < IaParams.n; i++) {
           buf->append(paramStrings->at(i));
           buf->append(":");
           buf->append( QString::number(IaParams.p[i]));
        }
        qDebug() << *buf;
        qDebug("---------------------------------------------------");
        if (status != GSL_SUCCESS){
            qDebug() << "ERROR: gsl failed, code:" << status << "size=" << size;
        }
        gsl_vector_free(x);
        gsl_vector_free(steps);
        gsl_multimin_fminimizer_free (solver);
    }
};

//  Diode
double dr_optimize::DiErr(const gsl_vector * xv, void *p) {
    float Ia;
    IaParams_t * param =  (IaParams_t *) p;
    //IaParams_t  optParam;
    for (int i=param->f; i < param->n; i++) param->o[i] = param->p[i] * fabs(gsl_vector_get(xv,i-param->f));

    int n = param->VaSteps+1;

    float sq_err =0,err;
    for(int i=0; i < n; i++) {
        results_t r = param->data->at(i);
        if (r.Ia!=-1) {
            if (!param->section_b) {
                Ia = DiIa(param, r.Va,0,0);
                err = (Ia - r.Ia);
                sq_err +=err*err;
                //Update data table
                r.IaMod=Ia;
            } else {
                Ia = DiIa(param, r.Vs,0,0);
                err = (Ia - r.Is);
                sq_err +=err*err;
                //Update data table
                r.IsMod=Ia;
            }
        }
        param->data->replace(i,r);
    }
    //qDebug() << "Ia err=" << sqrt(sq_err);
    return(sqrt(sq_err));
};

float dr_optimize::DiIa(IaParams_t * p, float Va, float Vs, float Vg) {
    float e = Va/p->o[KP];
    if (e<0) e=0;
    float Ia = pow(e, p->o[EX]);
    return(Ia);
}
//---------End Diode ------------


double dr_optimize::TriErr(const gsl_vector * xv, void *p) {
    float Ia;
    IaParams_t * param =  (IaParams_t *) p;
    for (int i=param->f; i < param->n; i++) param->o[i] = param->p[i] * fabs(gsl_vector_get(xv,i-param->f));
    int n = (param->VgSteps+1)*(param->VaSteps+1);

    float sq_err =0,err;
    for(int i=0; i < n; i++) {
        results_t r = param->data->at(i);
        if (r.Ia!=-1) {
            if (param->section_b) {
                Ia = TriIa(param, r.Vs,0, r.Vg);
                err = (Ia - r.Is);
            }
            else {
                Ia = TriIa(param, r.Va,0, r.Vg);
                err = (Ia - r.Ia);
            }
            sq_err +=err*err;
            //Update data table
            if (Ia<0 || Ia>1000) Ia=r.Ia;
            if (param->section_b) r.IsMod=Ia; else r.IaMod=Ia;
            param->data->replace(i,r);
        }
    }
    //qDebug() << "Ia err=" << sqrt(sq_err)/n;
    return(sqrt(sq_err));
}

float dr_optimize::TriIa(IaParams_t * p, float Va, float Vs, float Vg) {
    float e = Va/p->o[KP] * log( 1 + exp( p->o[KP]*(1/p->o[MU] + Vg/sqrt(p->o[KVB] +Va*Va) ) ) );
    if (e<0) e=0;
    return(1000*pow(e,p->o[EX])/p->o[KG1]);
}
//---- End of Triode -----


double dr_optimize::KpAErr(const gsl_vector * xv, void *p) {
    float Ia, Is;
    IaParams_t * param =  (IaParams_t *) p;
    int n = param->data->length();
    for (int i=param->f; i < param->n; i++) param->o[i] = param->p[i] * fabs(gsl_vector_get(xv,i-param->f));

    float sq_err =0,err;
    for(int i=0; i < n; i++) {
        results_t r = param->data->at(i);
        if (r.Ia!=-1) {
            Ia = KpIa(param, r.Va, r.Vs, r.Vg);
            err = (Ia - r.Ia);
            sq_err +=err*err;

            //Update data table
            //Sanity checks
            if (Ia<0 || Ia>1000) Ia=r.Ia;
            r.IaMod=Ia;
            param->data->replace(i,r);
        }
    }
    //qDebug() << "Ia err=" << sqrt(sq_err);
    return(sqrt(sq_err));
};

float dr_optimize::KpIa(IaParams_t * p, float Va, float Vg2, float Vg) {
    float e = Vg2/p->o[KP] * log( 1 + exp( p->o[KP]*(1/p->o[MU] + Vg/Vg2 ) ) );
    if (e<0) e=0;
    float Ipk =pow(e,p->o[EX]);
    float Ia = Ipk/p->o[KG1]*atan(Va/p->o[KVB]);
    return(1000*Ia);
}

double dr_optimize::KpSErr(const gsl_vector * xv, void *p) {
    float Ia, Is;
    IaParams_t * param =  (IaParams_t *) p;
    int n = param->data->length();
    for (int i=param->f; i < param->n; i++) param->o[i] = param->p[i] * fabs(gsl_vector_get(xv,i-param->f));

    float sq_err =0,err;
    for(int i=0; i < n; i++) {
        results_t r = param->data->at(i);
        if (r.Ia!=-1) {
            Is = KpIs(param, r.Va, r.Vs, r.Vg);
            err = (Is - r.Is);
            sq_err +=err*err;

            //Update data table
            //Sanity checks
            if (Is<0 || Is>1000) Is=r.Is;
            r.IsMod=Is;
            param->data->replace(i,r);
        }
    }
    //qDebug() << "Is err=" << sqrt(sq_err);
    return(sqrt(sq_err));
};

float dr_optimize::KpIs(IaParams_t * p, float Va, float Vg2, float Vg) {
    float e = Vg2/p->o[KP] * log( 1 + exp( p->o[KP]*(1/p->o[MU] + Vg/Vg2 ) ) );
    if (e<0) e=0;
    float Ipk =pow(e,p->o[EX]);
    float Is = Ipk/p->o[KG2];
    return(1000*Is);
}
//--- End Kp
//DERK P: Pentode with Ig2 model

double dr_optimize::DpErr(const gsl_vector * xv, void *p) {
    float Ia, Is;
    IaParams_t * param =  (IaParams_t *) p;
    int n = param->data->length();
    for (int i=param->f; i < param->n; i++) param->o[i] = param->p[i] * fabs(gsl_vector_get(xv,i-param->f));

    float sq_err =0,err;
    for(int i=0; i < n; i++) {
        results_t r = param->data->at(i);
        if (r.Ia!=-1) {
            Ia = DpIa(param, r.Va, param->data->at(i).Vs, r.Vg);
            err = (Ia - r.Ia);
            //err = Ia - r.Ia - r.Is ;
            sq_err +=err*err;

            Is = DpIs(param, r.Va, r.Vs, r.Vg);
            err = (Is - r.Is);
            sq_err +=err*err;

            //Update data table
            //Sanity checks
            if (Ia<0 || Ia>1000) Ia=-1;
            if (Is<0 || Is>1000) Is=-1;
            r.IsMod=Is;
            r.IaMod=Ia;
            param->data->replace(i,r);
        }
    }
    //qDebug() << "error" << sqrt(sq_err);
    return(sqrt(sq_err));
};

float dr_optimize::DpIa(IaParams_t * p, float Va, float Vg2, float Vg) {
    //Derk Reefman 'Theory' sec 4.4 eqn (17)
    float e = EK(Vg2,Vg, p->o[MU], p->o[KP], p->o[KVB]);
    if (e<0) e=0;
    float Ipk =pow(e,p->o[EX]);
    float Alpha = 1 - p->o[KG1]/p->o[KG2]*(1+ p->o[ALPHA_S]);
    float Ia = Ipk * (1/p->o[KG1] -1/p->o[KG2] +p->o[AP]*Va/p->o[KG1] -( Alpha/p->o[KG1] + p->o[ALPHA_S]/p->o[KG2])/(1+p->o[BETA]*Va));
    //float Ia = Ipk * 1/p->o[KG1] * (1 + p->o[AP]*Va - Alpha/(1+p->o[BETA]*Va));  //total
    return(1000*Ia);
}

float dr_optimize::DpIs(IaParams_t * p, float Va, float Vg2, float Vg) {
    //Derk Reefman 'Theory' sec 4.4 eqn (15)
    float e = EK(Vg2, Vg, p->o[MU], p->o[KP], p->o[KVB]);
    if (e<0) e=0;
    float Ipk =pow(e,p->o[EX]);
    float Ig2 = Ipk/p->o[KG2] *( 1 + p->o[ALPHA_S]/(1 + p->o[BETA]*Va));
    return(1000*Ig2);
}
//---end Dp -------------
//DERK-P: Beam Tetrode with Ig2 model
double dr_optimize::DEPErr(const gsl_vector * xv, void *p) {
    float Ia, Is;
    IaParams_t * param =  (IaParams_t *) p;
    int n = param->data->length();
    for (int i=param->f; i < param->n; i++) param->o[i] = param->p[i] * fabs(gsl_vector_get(xv,i-param->f));

    float sq_err =0,err;
    for(int i=0; i < n; i++) {
        results_t r = param->data->at(i);
        if (r.Ia!=-1) {
            Ia = DEPIa(param, r.Va, r.Vs, r.Vg);
            err = (Ia - r.Ia);
            sq_err +=err*err;

            Is = DEPIs(param, r.Va, r.Vs, r.Vg);
            err = (Is - r.Is);
            sq_err +=err*err;

            //Update data table
            //Sanity checks
            if (Ia<0 || Ia>1000) Ia=r.Ia;
            if (Is<0 || Is>1000) Is=r.Is;
            r.IsMod=Is;
            r.IaMod=Ia;
            param->data->replace(i,r);
        }
    }
    //qDebug() << "Ia err=" << sqrt(sq_err);
    return(sqrt(sq_err));
};

float dr_optimize::DEPIa(IaParams_t * p, float Va, float Vg2, float Vg) {
    //Derk 'Theory' eqn (22), sec 4.5
    float e = EK(Vg2,Vg, p->o[MU],p->o[KP],p->o[KVB]);
    if (e<0) e=0;
    float Ipk = pow(e,p->o[EX]);
    //Ia = Ipk*( 1/kg1 -1/Kg2 + Ap*Va/kg1 - (Alpha/Kg1 + Alpha_s/kg2)/(1+Beta*Va))
    float Alpha = 1 - p->o[KG1]/p->o[KG2]*(1+ p->o[ALPHA_S]);
    float Ia = Ipk * (1/p->o[KG1] -1/p->o[KG2] +p->o[AP]*Va/p->o[KG1]
                      -( Alpha/p->o[KG1] + p->o[ALPHA_S]/p->o[KG2])*exp(-(pow(p->o[BETA]*Va,1.5))));
    return(Ia*1000);
}

float dr_optimize::DEPIs(IaParams_t * p, float Va, float Vg2, float Vg) {
    //Derk 'Theory' eqn (20), sec 4.5
    float e = EK(Vg2,Vg, p->o[MU],p->o[KP],p->o[KVB]);
    if (e<0) e=0;
    float Ipk = pow(e,p->o[EX]);
    //Ig2 =  Ipk/Kg2* (1 + Alpha_s * exp( -(Beta*Va)^(3/2) ))
    float Ig2 = Ipk/p->o[KG2] *( 1 + p->o[ALPHA_S] * exp(-(pow(p->o[BETA]*Va,1.5))));
    return(Ig2*1000);
}
//-----end Dpe

//DERK B: Pentode + SE

double dr_optimize::DBErr(const gsl_vector * xv, void *p) {
    float Ia, Is;
    IaParams_t * param =  (IaParams_t *) p;
    int n = param->data->length();
    for (int i=param->f; i < param->n; i++) param->o[i] = param->p[i] * fabs(gsl_vector_get(xv,i-param->f));

    float sq_err =0,err;
    for(int i=0; i < n; i++) {
        results_t r = param->data->at(i);
        if (r.Ia!=-1) {
            Ia = DBIa(param, r.Va, r.Vs, r.Vg);
            err = (Ia - r.Ia);
            sq_err +=err*err;

            Is = DBIs(param, r.Va, r.Vs, r.Vg);
            err = (Is - r.Is);
            sq_err +=err*err;

            //Update data table
            //Sanity checks
            if (Ia<0 || Ia>1000) Ia=r.Ia;
            if (Is<0 || Is>1000) Is=r.Is;
            r.IsMod=Is;
            r.IaMod=Ia;
            param->data->replace(i,r);
        }
    }
    //qDebug() << "Ia err=" << sqrt(sq_err);
    return(sqrt(sq_err));
};

float dr_optimize::DBIa(IaParams_t * p, float Va, float Vg2, float Vg) {
    //Derk Reefman 'Theory' Sec 5.3 eqn (28)
    float e = EK(Vg2,Vg, p->o[MU],p->o[KP],p->o[KVB]);
    if (e<0) e=0;
    float Ipk =pow(e,p->o[EX]);
    float Alpha = 1 - p->o[KG1]/p->o[KG2]*(1+ p->o[ALPHA_S]);
    //Psec = S * Va(1 + tanh( -Ap*(Va -(Vg2/lamda -v*Vg1 -w))) //eqn (25)
    //float Vy = (Vg2/p->o[LAMDA] -p->o[NU]*Vg - p->o[OMEGA]);
    //float Vx = (Va - (Vg2/p->o[LAMDA] -p->o[NU]*Vg - p->o[OMEGA]));
    //float t= tanh(-p->o[ASUBP] *Vx);
    //float Psec = p->o[S_] * Va * (1+t);
    float Psec = p->o[S_] * Va * ( 1 + tanh(-p->o[ASUBP] *(Va - (Vg2/p->o[LAMDA] -p->o[NU]*Vg - p->o[OMEGA])) ));
    //Ia       Ipk  *(      1/kg1 -     1/Kg2 +      Ap *Va/     kg1 - Psec/Kg2 -1/(1+Beta*Va)( Alpha/Kg1 + Alpha_s/kg2))
    float Ia = Ipk * (1/p->o[KG1] -1/p->o[KG2] +p->o[AP]*Va/p->o[KG1]
                      - Psec/p->o[KG2] -1/(1+p->o[BETA]*Va)*( Alpha/p->o[KG1] + p->o[ALPHA_S]/p->o[KG2]));
    return(1000*Ia);
}

float dr_optimize::DBIs(IaParams_t * p, float Va, float Vg2, float Vg) {
    //Derk Reefman 'Theory' Sec 5.3 eqn (27)
    float e = EK(Vg2,Vg, p->o[MU],p->o[KP],p->o[KVB]);
    if (e<0) e=0;
    float Ipk =pow(e,p->o[EX]);
    float Psec = p->o[S_] * Va * ( 1 + tanh(-p->o[ASUBP] *(Va - (Vg2/p->o[LAMDA] -p->o[NU]*Vg - p->o[OMEGA])) )); //eqn(25)
    //Ig2 = Ipk/Kg2*(1 +  Alpha_s/(1+Beta*Va) +Psec)
    float Ig2 = Ipk / p->o[KG2] * (1 + p->o[ALPHA_S]/(1 + p->o[BETA] * Va) + Psec);
    return(1000*Ig2);
}

//--- end Dbt --------

//DERKE B: Beam tetrode with Ig2 model and secondary emission

double dr_optimize::DEBErr(const gsl_vector * xv, void *p) {
    float Ia, Is;
    IaParams_t * param =  (IaParams_t *) p;
    //IaParams_t optParam;
    int n = param->data->length();
    for (int i=param->f; i < param->n; i++) param->o[i] = param->p[i] * fabs(gsl_vector_get(xv,i-param->f));

    float sq_err =0,err;
    for(int i=0; i < n; i++) {
        results_t r = param->data->at(i);
        if (r.Ia!=-1) {
            Ia = DEBIa(param, r.Va, r.Vs, r.Vg);
            err = (Ia - r.Ia);
            sq_err +=err*err;

            Is = DEBIs(param, r.Va, r.Vs, r.Vg);
            err = (Is - r.Is);
            sq_err +=err*err;

            //Update data table
            //Sanity checks
            if (Ia<0 || Ia>1000) Ia=r.Ia;
            if (Is<0 || Is>1000) Is=r.Is;
            r.IsMod=Is;
            r.IaMod=Ia;
            param->data->replace(i,r);
        }
    }
    //qDebug() << "Ia err=" << sqrt(sq_err);
    return(sqrt(sq_err));
};

float dr_optimize::DEBIa(IaParams_t * p, float Va, float Vg2, float Vg) {
    //Derk Reefman 'Theory' Sec 5.4 eqn (30) (25)
    float e = EK(Vg2,Vg, p->o[MU],p->o[KP],p->o[KVB]);
    if (e<0) e=0;
    float Ipk =pow(e,p->o[EX]);
    float Alpha = 1 - p->o[KG1]/p->o[KG2]*(1+ p->o[ALPHA_S]);
    //Psec = S * Va(1 + tanh( -Ap*(Va -(Vg2/lamda -v*Vg1 -w)))
    float Psec = p->o[S_] * Va * ( 1 + tanh(-p->o[ASUBP] *(Va - (Vg2/p->o[LAMDA] -p->o[NU]*Vg - p->o[OMEGA])) )); //eqn(25)
    //Ia =     Ipk *(       1/Kg1 -     1/Kg2 +      Ap *Va/     Kg1  - Psec/     Kg2  -exp( -(Beta*Va)^(3/2) )     *(Alpha/Kg1        + Alpha_s/Kg2))
    float Ia = Ipk * (1/p->o[KG1] -1/p->o[KG2] +p->o[AP]*Va/p->o[KG1] - Psec/p->o[KG2] -exp(-pow(p->o[BETA]*Va,1.5))*(Alpha/p->o[KG1] +p->o[ALPHA_S]/p->o[KG2]));
    return(1000*Ia);
}

float dr_optimize::DEBIs(IaParams_t * p, float Va, float Vg2, float Vg) {
    //Derk Reefman 'Theory' Sec 5.4 eqn (29) (25)
    float e = EK(Vg2,Vg, p->o[MU],p->o[KP],p->o[KVB]);
    if (e<0) e=0;
    float Ipk =pow(e,p->o[EX]);
    float Alpha = 1 - p->o[KG1]/p->o[KG2]*(1+p->o[ALPHA_S]);
    float Psec = p->o[S_] * Va * ( 1 + tanh(-p->o[ASUBP] *(Va - (Vg2/p->o[LAMDA] -p->o[NU]*Vg - p->o[OMEGA])) )); //eqn (25)
    //Ig2 =     Ipk/      Kg2  * (1 + Alpha_s*exp( -(Beta*Va)^3/2) ) +Psec)
    float Ig2 = Ipk/ p->o[KG2] * (1 + p->o[ALPHA_S] * exp(- pow(p->o[BETA]*Va,1.5) + Psec));
    return(1000*Ig2);
}

//--- end Dbte --------


void dr_optimize::Save_Spice_Model(QWidget * widget, QByteArray tnameba) {
    char * tname = tnameba.data() ;
    static QString oldFileName;
    qDebug() << "Save path:" << oldFileName;
    if (oldFileName.length()==0) oldFileName=QDir::homePath();
    QString dataFileName = QFileDialog::getSaveFileName(widget, QObject::tr("Save Spice .cir file"), oldFileName,
      "Circuit files (*.cir)",
      new QString("Circuit files (*.cir)"));
    if (dataFileName!="") {
        int fs= dataFileName.lastIndexOf("/");
        oldFileName = dataFileName.mid(0,fs);
        //Save the model
        if (IaParams.data->length()>0) {
            QFile datafile(dataFileName);
            if (datafile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                char m[512];
                //Info
                sprintf(m,"* library format: LTSpice\n");
                datafile.write(m);
                //Header
                //float mu,ex,kg1,kp,kvb;
                //float I0, s, mup, mus, mug, ex;
                switch (IaParams.penfn) {
                    case DIODE:
                    {
                        datafile.write("****************************************************\n");
                        datafile.write("* Diode model parameters created with uTmax www.bmamps.com\n");
                        sprintf(m,".SUBCKT %s 1 2 ; P G C (Triode)\n",tname);
                        datafile.write(m);
                        sprintf(m," X1 1 2 Diode EX=%f KP=%f RGI=2000 CCP=0 ;\n",
                            IaParams.o[EX],
                            IaParams.o[KP]);
                        datafile.write(m);
                        sprintf(m, ".ENDS  %s\n",tname);
                        datafile.write(m);

                        datafile.write("****************************************************\n");
                        datafile.write(".SUBCKT Diode 1 2; A G1 C\n");
                        datafile.write("G1  1 2 VALUE={(PWR(V(1,2)/KP,EX))}\n");
                        datafile.write("C3  1 2 {CCP}  ; CATHODE-PLATE\n");
                        datafile.write(".ENDS Diode\n");
                        break;
                    }
                    case DUAL_DIODE:
                    {
                        datafile.write("****************************************************\n");
                        datafile.write("* Diode model parameters created with uTmax www.bmamps.com\n");
                        sprintf(m,".SUBCKT %s-a 1 2 ; P C (Diode)\n",tname);
                        datafile.write(m);
                        sprintf(m," X1 1 2 Diode EX=%f KP=%f RGI=2000 CCP=0 ;\n",
                            IaParams.o[EX],
                            IaParams.o[KP]);
                        datafile.write(m);
                        sprintf(m, ".ENDS  %s-a\n",tname);
                        datafile.write(m);

                        sprintf(m,".SUBCKT %s-b 1 2 ; P C (Diode)\n",tname);
                        datafile.write(m);
                        sprintf(m," X1 1 2 Diode EX=%f KP=%f RGI=2000 CCP=0 ;\n",
                            IbParams.o[EX],
                            IbParams.o[KP]);
                        datafile.write(m);
                        sprintf(m, ".ENDS  %s-b\n",tname);
                        datafile.write(m);

                        datafile.write("****************************************************\n");
                        datafile.write(".SUBCKT Diode 1 2; A G1 C\n");
                        datafile.write("G1  1 2 VALUE={(PWR(V(1,2)/KP,EX))}\n");
                        datafile.write("C3  1 2 {CCP}  ; CATHODE-PLATE\n");
                        datafile.write(".ENDS Diode\n");
                        datafile.write(m);
                        break;
                    }
                    case TRIODE:
                    {
                        datafile.write("****************************************************\n");
                        datafile.write("* Triode model parameters created with uTmax www.bmamps.com\n");
                        sprintf(m,".SUBCKT %s 1 2 3 ; P G C (Triode) ; TRIODE SECTION\n",tname);
                        datafile.write(m);
                        sprintf(m," X1 1 2 3 Triode MU=%.2f EX=%.2f KG1=%.1f KP=%.2f KVB=%.2f VCT=0 RGI=2000 CCG=0p CGP=0p CCP=0p ;\n",
                            IaParams.o[MU],
                            IaParams.o[EX],
                            IaParams.o[KG1],
                            IaParams.o[KP],
                            IaParams.o[KVB],
                            "0","0","0\n");
                        datafile.write(m);
                        sprintf(m, ".ENDS  %s\n",tname);
                        datafile.write(m);
                        datafile.write("****************************************************\n");
                        datafile.write("* Triode model (c) Norman Koren, used with permission\n");
                        datafile.write(".SUBCKT Triode 1 2 3; A G1 C\n");
                        datafile.write("E1  7 0 VALUE={V(1,3)/KP*LOG(1+EXP(KP*(1/MU+(V(2,3)+VCT)/SQRT(KVB+V(1,3)*V(1,3)))))}\n");
                        datafile.write("RE1 7 0 1G\n");
                        datafile.write("G1  1 3 VALUE={0.5*(PWR(V(7,0),EX)+PWRS(V(7,0),EX))/KG1}\n");
                        datafile.write("RCP 1 3 1G    ; TO AVOID FLOATING NODES IN MU-FOLLOWER\n");
                        datafile.write("C1  2 3 {CCG}  ; CATHODE-GRID\n");
                        datafile.write("C2  2 1 {CGP}  ; GRID=PLATE\n");
                        datafile.write("C3  1 3 {CCP}  ; CATHODE-PLATE\n");
                        datafile.write("D3  5 3 DX     ; FOR GRID CURRENT\n");
                        datafile.write("R1  2 5 {RGI}  ; FOR GRID CURRENT\n");
                        datafile.write(".MODEL DX D(IS=1N RS=1 CJO=10PF TT=1N)\n");
                        datafile.write(".ENDS Triode\n");
                        break;
                    }
                    case DUAL_TRIODE:
                    {
                        datafile.write("****************************************************\n");
                        datafile.write("* Triode model parameters created with uTmax www.bmamps.com\n");
                        sprintf(m,"* KOREN Triode model\n");
                        datafile.write(m);
                        sprintf(m,".SUBCKT %s-a 1 2 3 ; P G C (Triode) ; TRIODE SECTION\n",tname);
                        datafile.write(m);
                        sprintf(m," X1 1 2 3 Triode MU=%.2f EX=%.2f KG1=%.1f KP=%.2f KVB=%.1f VCT=0 RGI=2000 CCG=0p CGP=0p CCP=0p\n",
                            IaParams.o[MU],
                            IaParams.o[EX],
                            IaParams.o[KG1],
                            IaParams.o[KP],
                            IaParams.o[KVB]);
                        datafile.write(m);
                        sprintf(m, ".ENDS  %s-a\n",tname);
                        datafile.write(m);
                        datafile.write("****************************************************\n");

                        datafile.write("* Triode model parameters created with uTmax www.bmamps.com\n");
                        sprintf(m,"* KOREN Triode model\n");
                        datafile.write(m);
                        sprintf(m,".SUBCKT %s-b 1 2 3 ; P G C (Triode) ; TRIODE SECTION\n",tname);
                        datafile.write(m);
                        sprintf(m," X1 1 2 3 Triode MU=%.2f EX=%.2f KG1=%.1f KP=%.2f KVB=%.1f VCT=0 RGI=2000 CCG=0p CGP=0p CCP=0p\n",
                            IbParams.o[MU],
                            IbParams.o[EX],
                            IbParams.o[KG1],
                            IbParams.o[KP],
                            IbParams.o[KVB]);
                        datafile.write(m);
                        sprintf(m, ".ENDS  %s-b\n",tname);
                        datafile.write(m);

                        datafile.write("****************************************************\n");
                        datafile.write("* Triode model (c) Norman Koren, used with permission\n");
                        datafile.write(".SUBCKT Triode 1 2 3; A G1 C\n");
                        datafile.write("E1  7 0 VALUE={V(1,3)/KP*LOG(1+EXP(KP*(1/MU+(V(2,3)+VCT)/SQRT(KVB+V(1,3)*V(1,3)))))}\n");
                        datafile.write("RE1 7 0 1G\n");
                        datafile.write("G1  1 3 VALUE={0.5*(PWR(V(7,0),EX)+PWRS(V(7,0),EX))/KG1}\n");
                        datafile.write("RCP 1 3 1G    ; TO AVOID FLOATING NODES IN MU-FOLLOWER\n");
                        datafile.write("C1  2 3 {CCG}  ; CATHODE-GRID\n");
                        datafile.write("C2  2 1 {CGP}  ; GRID=PLATE\n");
                        datafile.write("C3  1 3 {CCP}  ; CATHODE-PLATE\n");
                        datafile.write("D3  5 3 DX     ; FOR GRID CURRENT\n");
                        datafile.write("R1  2 5 {RGI}  ; FOR GRID CURRENT\n");
                        datafile.write(".MODEL DX D(IS=1N RS=1 CJO=10PF TT=1N)\n");
                        datafile.write(".ENDS Triode\n");
                        break;
                    }
                    case KOREN_P:
                    {
                        datafile.write("****************************************************\n");
                        datafile.write("Pentode model parameters created with uTmax www.bmamps.com\n");
                        sprintf(m,"* KOREN Pentode with elementary screen current model\n");
                        datafile.write(m);
                        sprintf(m,".SUBCKT %s 1 2 3 4 ; P G1 G1 C (Pentode) ; PENTODE SECTION\n",tname);
                        datafile.write(m);
                        sprintf(m," X1 1 2 3 4 PENTODE1 MU=%f EX=%f KG1=%f KP=%f KVB=%f KG2=%f RGI=2000 CCG=0p CPG1=0p CCP=0p \n",
                            IaParams.o[MU],
                            IaParams.o[EX],
                            IaParams.o[KG1],
                            IaParams.o[KP],
                            IaParams.o[KVB],
                            IaParams.o[KG2]);
                        datafile.write(m);
                        sprintf(m, ".ENDS  %s\n",tname);
                        datafile.write(m);
                        datafile.write("****************************************************\n");
                        datafile.write(".SUBCKT PENTODE1 1 2 3 4 ; A G2 G1 C\n");
                        datafile.write("RE1  7 0  1MEG    ; DUMMY SO NODE 7 HAS 2 CONNECTIONS\n");
                        datafile.write("E1   7 0  VALUE={V(2,4)/KP * LOG(1+EXP(( 1/MU + V(3,4)/V(2,4) )*KP))}\n");
                        datafile.write("G1   1 4  VALUE={(PWR(V(7),EX)+PWRS(V(7),EX))/2/KG1 * ATAN(V(1,4)/KVB)}\n");
                        datafile.write("G2   2 4  VALUE={(PWR(V(7),EX)+PWRS(V(7),EX))/2/KG2}\n");
                        datafile.write("RCP  1 4  1G      ; FOR CONVERGENCE	A  - C\n");
                        datafile.write("C1   3 4  {CCG}   ; CATHODE-GRID 1	C  - G1\n");
                        datafile.write("C2   1 3  {CPG1}  ; GRID 1-PLATE	G1 - A\n");
                        datafile.write("C3   1 4  {CCP}   ; CATHODE-PLATE	A  - C\n");
                        datafile.write("R1   3 5  {RGI}   ; FOR GRID CURRENT	G1 - 5\n");
                        datafile.write("D3   5 4  DX      ; FOR GRID CURRENT	5  - C\n");
                        datafile.write(".MODEL DX D(IS=1N RS=1 CJO=10PF TT=1N)\n");
                        datafile.write(".ENDS PENTODE1\n");
                        break;
                    }
                    case DERK_P:
                    {
                        datafile.write("****************************************************\n");
                        datafile.write("* PentodeD model parameters created with uTmax www.bmamps.com\n");
                        datafile.write(m);
                        sprintf(m,"* DERK Pentode with screen current model\n");
                        datafile.write(m);
                        sprintf(m,".SUBCKT %s 1 2 3 4 ; P G2 G1 C\n",tname);
                        datafile.write(m);
                        datafile.write(" X1 1 2 3 4 PentodeD\n");
                        sprintf(m,"+MU=%f EX=%f KG1=%f KP=%f KVB=%f KG2=%f als=%f be=%f Aokg1=%f Ookg1mOokG2=%f\n \
                                  + RGI=2000 CCG1=0.0P  CCG2 = 0.0p CPG1 = 0.0p CG1G2 = 0.0p CCP=0.0P \n",
                                IaParams.o[MU],
                                IaParams.o[EX],
                                IaParams.o[KG1],
                                IaParams.o[KP],
                                IaParams.o[KVB],
                                IaParams.o[KG2],
                                IaParams.o[ALPHA_S],
                                IaParams.o[BETA],
                                IaParams.o[AP]/IaParams.o[KG1],
                                1/IaParams.o[KG1]-1/IaParams.o[KG2] );
                        datafile.write(m);
                        sprintf(m, ".ENDS  %s\n",tname);
                        datafile.write(m);

                        datafile.write("****************************************************\n");
                        datafile.write(".SUBCKT PentodeD 1 2 3 4; A G2 G1 C\n");
                        datafile.write("RE1  7 0  1MEG    ; DUMMY SO NODE 7 HAS 2 CONNECTIONS\n");
                        datafile.write("E1 7 0 VALUE=\n");
                        datafile.write("+{V(2,4)/KP*LOG(1+EXP(KP*(1/MU+V(3,4)/SQRT(KVB+V(2,4)*V(2,4)))))}\n");
                        datafile.write("E2   8 0 VALUE = {Ookg1mOokG2*(1-1/(1 + be*V(1,4))) + Aokg1*V(1,4)}\n");
                        datafile.write("G1   1 4 VALUE = {0.5*(PWR(V(7),EX)+PWRS(V(7),EX))*V(8)}\n");
                        datafile.write("G2   2 4 VALUE = {0.5*(PWR(V(7),EX)+PWRS(V(7),EX))/KG2 * (1+ als/(1+be*V(1,4)))}\n");
                        datafile.write("RCP  1 4  1G      ; FOR CONVERGENCE	A  - C\n");
                        datafile.write("C1   3 4  {CCG1}   ; CATHODE-GRID 1	C  - G1\n");
                        datafile.write("C4   2 4  {CCG2}   ; CATHODE-GRID 2	C  - G2\n");
                        datafile.write("C5   2 3  {CG1G2}   ; GRID 1 -GRID 2	G1  - G2\n");
                        datafile.write("C2   1 3  {CPG1}  ; GRID 1-PLATE	G1 - A\n");
                        datafile.write("C3   1 4  {CCP}   ; CATHODE-PLATE	A  - C\n");
                        datafile.write("R1   3 5  {RGI}   ; FOR GRID CURRENT	G1 - 5\n");
                        datafile.write("D3   5 4  DX      ; FOR GRID CURRENT	5  - C\n");
                        datafile.write(".MODEL DX D(IS=1N RS=1 CJO=10PF TT=1N)\n");
                        datafile.write(".ENDS PenthodeD\n");
                        break;
                    }
                    case DERKE_P:
                    {
                        datafile.write("****************************************************\n");
                        datafile.write("* Pentode model parameters created with uTmax www.bmamps.com\n");
                        sprintf(m,"* DERKE Pentode with screen current and secondary emission model\n\n");
                        datafile.write(m);
                        sprintf(m,".SUBCKT %s 1 2 3 4 ; P G2 G1 C\n",tname);
                        datafile.write(m);
                        datafile.write(" X1 1 2 3 4 PentodeE\n\n");
                        sprintf(m,"+MU=%f EX=%f KG1=%f KP=%f KVB=%f KG2=%f als=%f be=%f Aokg1=%f Ookg1mOokG2=%f\n \
                                  + RGI=2000 CCG1=0.0P  CCG2 = 0.0p CPG1 = 0.0p CG1G2 = 0.0p CCP=0.0P \n",
                                IaParams.o[MU],
                                IaParams.o[EX],
                                IaParams.o[KG1],
                                IaParams.o[KP],
                                IaParams.o[KVB],
                                IaParams.o[KG2],
                                IaParams.o[ALPHA_S],
                                IaParams.o[BETA],
                                IaParams.o[AP]/IaParams.o[KG1],
                                1/IaParams.o[KG1]-1/IaParams.o[KG2] );
                        datafile.write(m);
                        sprintf(m, ".ENDS  %s\n",tname);
                        datafile.write(m);
                        datafile.write("****************************************************\n");
                        datafile.write(".SUBCKT PentodeE 1 2 3 4; A G2 G1 C\n");
                        datafile.write("RE1  7 0  1MEG    ; DUMMY SO NODE 7 HAS 2 CONNECTIONS\n");
                        datafile.write("E1 7 0 VALUE=\n");
                        datafile.write("+{V(2,4)/KP*LOG(1+EXP(KP*(1/MU+V(3,4)/SQRT(KVB+V(2,4)*V(2,4)))))}\n");
                        datafile.write("E2   8 0 VALUE = {Ookg1mOokG2*(1 - exp( PWR(-be*V(1,4), 1.5 ) ) ) + Aokg1*V(1,4)}\n");
                        datafile.write("G1   1 4 VALUE = {0.5*(PWR(V(7),EX)+PWRS(V(7),EX))*V(8)}\n");
                        datafile.write("G2   2 4 VALUE = {0.5*(PWR(V(7),EX)+PWRS(V(7),EX))/KG2 * (1+ als/exp( PWR(-be*V(1,4), 1.5 ) ) )}\n");
                        datafile.write("RCP  1 4  1G      ; FOR CONVERGENCE	A  - C\n");
                        datafile.write("C1   3 4  {CCG1}   ; CATHODE-GRID 1	C  - G1\n");
                        datafile.write("C4   2 4  {CCG2}   ; CATHODE-GRID 2	C  - G2\n");
                        datafile.write("C5   2 3  {CG1G2}   ; GRID 1 -GRID 2	G1  - G2\n");
                        datafile.write("C2   1 3  {CPG1}  ; GRID 1-PLATE	G1 - A\n");
                        datafile.write("C3   1 4  {CCP}   ; CATHODE-PLATE	A  - C\n");
                        datafile.write("R1   3 5  {RGI}   ; FOR GRID CURRENT	G1 - 5\n");
                        datafile.write("D3   5 4  DX      ; FOR GRID CURRENT	5  - C\n");
                        datafile.write(".MODEL DX D(IS=1N RS=1 CJO=10PF TT=1N)\n");
                        datafile.write(".ENDS PenthodeE\n");
                        break;
                    }
                    case DERK_B:
                    {
                        datafile.write("****************************************************\n");
                        datafile.write("* Beam Tetrode model parameters created with uTmax www.bmamps.com\n");
                        sprintf(m,"* DERKE Beam Tetrode with screen current model\n");
                        datafile.write(m);
                        sprintf(m,".SUBCKT %s 1 2 3 4 ; P G2 G1 C\n",tname);
                        datafile.write(m);
                        datafile.write(" X1 1 2 3 4 BTetrode\n");
                        sprintf(m,"+ MU=%f EX=%f KG1=%f KP=%f KVB=%f KG2=%f\n \
+ als=%f be=%f ap=%f Aokg1=%f Ookg1mOokG2=%f sc=%f\n+ lam=%f w=%f nu=%f\n \
+ RGI=2000 CCG1=0.0P  CCG2 = 0.0p CPG1 = 0.0p CG1G2 = 0.0p CCP=0.0P \n",
                                IaParams.o[MU],
                                IaParams.o[EX],
                                IaParams.o[KG1],
                                IaParams.o[KP],
                                IaParams.o[KVB],
                                IaParams.o[KG2],
                                IaParams.o[ALPHA_S],
                                IaParams.o[BETA],
                                IaParams.o[ASUBP],
                                IaParams.o[AP]/IaParams.o[KG1],
                                1/IaParams.o[KG1]-1/IaParams.o[KG2],
                                IaParams.o[S_],
                                IaParams.o[LAMDA],
                                IaParams.o[OMEGA],
                                IaParams.o[NU] );
                        datafile.write(m);
                        sprintf(m, ".ENDS  %s\n",tname);
                        datafile.write(m);
                        datafile.write("****************************************************\n");
                        datafile.write(".SUBCKT BTetrode 1 2 3 4; A G2 G1 C\n");
                        datafile.write("RE1  7 0  1MEG    ; DUMMY SO NODE 7 HAS 2 CONNECTIONS\n");
                        datafile.write("E1 7 0 VALUE=\n");
                        datafile.write("+{V(2,4)/KP*LOG(1+EXP(KP*(1/MU+V(3,4)/SQRT(KVB+V(2,4)*V(2,4)))))}\n");
                        datafile.write("E2   8 0 VALUE = {Ookg1mOokG2*(1 - 1/(1+be*V(1,4))) + Aokg1*V(1,4) - V(9)/KG2}\n");
                        datafile.write("E3   9 0 VALUE = {Sc*V(1,4)*(1+tanh(-ap*(V(1,4)-V(2,4)/lam+w+nu*V(3,4))))}\n");
                        datafile.write("G1   1 4 VALUE = {0.5*(PWR(V(7),EX)+PWRS(V(7),EX))*V(8)}\n");
                        datafile.write("G2   2 4 VALUE = {0.5*(PWR(V(7),EX)+PWRS(V(7),EX))/KG2 * (1+ als/(1+be*V(1,4)) +V(9) )}\n");
                        datafile.write("RCP  1 4  1G      ; FOR CONVERGENCE	A  - C\n");
                        datafile.write("C1   3 4  {CCG1}   ; CATHODE-GRID 1	C  - G1\n");
                        datafile.write("C4   2 4  {CCG2}   ; CATHODE-GRID 2	C  - G2\n");
                        datafile.write("C5   2 3  {CG1G2}   ; GRID 1 -GRID 2	G1  - G2\n");
                        datafile.write("C2   1 3  {CPG1}  ; GRID 1-PLATE	G1 - A\n");
                        datafile.write("C3   1 4  {CCP}   ; CATHODE-PLATE	A  - C\n");
                        datafile.write("R1   3 5  {RGI}   ; FOR GRID CURRENT	G1 - 5\n");
                        datafile.write("D3   5 4  DX      ; FOR GRID CURRENT	5  - C\n");
                        datafile.write(".MODEL DX D(IS=1N RS=1 CJO=10PF TT=1N)\n");
                        datafile.write(".ENDS BTetrode\n");
                        break;
                    }
                    case DERKE_B:
                    {
                        datafile.write("****************************************************\n");
                        datafile.write("* Beam Tetrode model parameters created with uTmax www.bmamps.com\n");
                        sprintf(m,"* DERKBE Beam Tetrode with screen current and secondary emission model\n");
                        datafile.write(m);
                        sprintf(m,".SUBCKT %s 1 2 3 4 ; P G2 G1 C\n",tname);
                        datafile.write(m);
                        datafile.write(" X1 1 2 3 4 BTetrodeE\n");
                        sprintf(m,"+ MU=%f EX=%f KG1=%f KP=%f KVB=%f KG2=%f als=%f be=%f\n \
+ ap=%f Aokg1=%f Ookg1mOokG2=%f sc=%f lam=%f w=%f nu=%f\n \
+ RGI=2000 CCG1=0.0P  CCG2 = 0.0p CPG1 = 0.0p CG1G2 = 0.0p CCP=0.0P \n",
                                IaParams.o[MU],
                                IaParams.o[EX],
                                IaParams.o[KG1],
                                IaParams.o[KP],
                                IaParams.o[KVB],
                                IaParams.o[KG2],
                                IaParams.o[ALPHA_S],
                                IaParams.o[BETA],
                                IaParams.o[ASUBP],
                                IaParams.o[AP]/IaParams.o[KG1],
                                1/IaParams.o[KG1]-1/IaParams.o[KG2],
                                IaParams.o[S_],
                                IaParams.o[LAMDA],
                                IaParams.o[OMEGA],
                                IaParams.o[NU] );
                        datafile.write(m);
                        sprintf(m, ".ENDS  %s\n",tname);
                        datafile.write(m);
                        datafile.write("****************************************************\n");
                        datafile.write(".SUBCKT BTetrodeE 1 2 3 4; A G2 G1 C\n");
                        datafile.write("RE1  7 0  1MEG    ; DUMMY SO NODE 7 HAS 2 CONNECTIONS\n");
                        datafile.write("E1 7 0 VALUE=\n");
                        datafile.write("+{V(2,4)/KP*LOG(1+EXP(KP*(1/MU+V(3,4)/SQRT(KVB+V(2,4)*V(2,4)))))}\n");
                        datafile.write("E2   8 0 VALUE = {Ookg1mOokG2*(1 - exp( -PWR(be*V(1,4), 1.5 ) ) ) + Aokg1*V(1,4) -V(9)/KG2}\n");
                        datafile.write("E3   9 0 VALUE = {Sc*V(1,4)*(1+tanh(-ap*(V(1,4)-V(2,4)/lam+w+nu*V(3,4))))}\n");
                        datafile.write("G1   1 4 VALUE = {0.5*(PWR(V(7),EX)+PWRS(V(7),EX))*V(8)}\n");
                        datafile.write("G2   2 4 VALUE = {0.5*(PWR(V(7),EX)+PWRS(V(7),EX))/KG2 * (1+ als/exp( PWR(-be*V(1,4), 1.5 ) ) +V(9)) }\n");
                        datafile.write("RCP  1 4  1G      ; FOR CONVERGENCE	A  - C\n");
                        datafile.write("C1   3 4  {CCG1}   ; CATHODE-GRID 1	C  - G1\n");
                        datafile.write("C4   2 4  {CCG2}   ; CATHODE-GRID 2	C  - G2\n");
                        datafile.write("C5   2 3  {CG1G2}   ; GRID 1 -GRID 2	G1  - G2\n");
                        datafile.write("C2   1 3  {CPG1}  ; GRID 1-PLATE	G1 - A\n");
                        datafile.write("C3   1 4  {CCP}   ; CATHODE-PLATE	A  - C\n");
                        datafile.write("R1   3 5  {RGI}   ; FOR GRID CURRENT	G1 - 5\n");
                        datafile.write("D3   5 4  DX      ; FOR GRID CURRENT	5  - C\n");
                        datafile.write(".MODEL DX D(IS=1N RS=1 CJO=10PF TT=1N)\n");
                        datafile.write(".ENDS BTetrodeE\n");
                        break;
                    }
                        default:
                        break;
                    }
                datafile.close();
            }
        }
    }
}

