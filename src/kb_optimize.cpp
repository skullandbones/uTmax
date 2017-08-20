#include "kb_optimize.h"
#include <QFile>
#include <QString>
#include <QDebug>
#include <QFileDialog>
#include <QtCore/qmath.h>



kb_optimize::kb_optimize()
{
}

kb_optimize::~kb_optimize()
{
}

//=========== SPICE model optimization =================
//fit a smooth curve through the given y vector

void kb_optimize::SetUpModel(QList<results_t> * dataSet, int Penfun, int Weight) {
    //Ia Params
    if (Penfun==0) {
        //Guess for a triode
        IaParams.penfn = Penfun;
        IaParams.data=dataSet;
        IaParams.ex = 1.6;
        IaParams.kg1=1000;
//        IaParams.kg1=powf(1000,1/IaParams.ex);
        IaParams.kp=600;
        IaParams.kvb=300;
        IaParams.gp=0;
        IaParams.max_vg=0;
        IaParams.mu=50;
        IaParams.weight = Weight;
        IaParams.vct=0;
    }
     else {
        //guess for pentode
        IaParams.penfn = Penfun;
        IaParams.data=dataSet;
        IaParams.ex = 1.5;
        IaParams.kg1=powf(1000,1/IaParams.ex);
        IaParams.kp=10;
        IaParams.kvb=30;
        IaParams.gp=0.0002;
        IaParams.max_vg=0;
        IaParams.mu=20;
        IaParams.weight = Weight;
        IaParams.vct=0;
        //Is params
        IsParams.penfn = Penfun;
        IsParams.data=dataSet;
        IsParams.I0=0.6;
        IsParams.s=10;
        IsParams.mup=0.2;
        IsParams.mus=0.2;
        IsParams.mug=0.2;
        IsParams.ex = 1.6;
        IsParams.weight = Weight;
     }
}

void kb_optimize::Optimize() {

    int n  = IaParams.data->length();
    int iter, status;
    if  (n > 6) {
        //Minimize Ia errors
        int max_iter=1000;
        int num_params=6;
        double size;
        //init starting point
        gsl_vector * x = gsl_vector_alloc(num_params);
        gsl_vector_set_all(x,1.0);
        //init step size
        gsl_vector * steps = gsl_vector_alloc(num_params);
        gsl_vector_set_all(steps, 0.01);
        gsl_multimin_function f;
        f.f = (double (* ) (const gsl_vector * x, void * params)) &kb_optimize::IaCalcErr;
        f.n = num_params;
        f.params = (void *) & IaParams;
        const gsl_multimin_fminimizer_type  * T = gsl_multimin_fminimizer_nmsimplex;
        gsl_multimin_fminimizer  * solver = NULL;
        solver = gsl_multimin_fminimizer_alloc(T,num_params);
        gsl_multimin_fminimizer_set(solver, &f, x, steps);
        iter =0;
        qDebug() << "Optimizing Ia";
        do {
              iter++;
              status = gsl_multimin_fminimizer_iterate(solver);
              if (status) break;
              size = gsl_multimin_fminimizer_size(solver);
              status = gsl_multimin_test_size(size, 1e-4);
              if (status == GSL_SUCCESS)
              {
                  qDebug() <<"converged to minimum at"
                  << "mu" << gsl_vector_get(solver->x, 0) * IaParams.mu
                  << "ex" << gsl_vector_get(solver->x, 1) * IaParams.ex
                  << "kg1" << gsl_vector_get(solver->x, 2) * IaParams.kg1
                  << "kp" << gsl_vector_get(solver->x, 3) * IaParams.kp
                  << "kvb" << gsl_vector_get(solver->x, 4) * IaParams.kvb
                  << "gp" << gsl_vector_get(solver->x, 5) * IaParams.gp;
                  //<< "kg2" << IaParams.kg2
                  //<< "x0" << gsl_vector_get(solver->x, 0)
                  //<< "x1" << gsl_vector_get(solver->x, 1)
                  //<< "x2" << gsl_vector_get(solver->x, 2)
                  //<< "x3" << gsl_vector_get(solver->x, 3)
                  //<< "x4" << gsl_vector_get(solver->x, 4);
              }
              //qDebug() <<"iter" << iter << "fval=" << solver->fval << "size=" << size;
            } while (status == GSL_CONTINUE && iter < max_iter);
        if (status != GSL_SUCCESS){
            qDebug() << "ERROR: gsl failed, code:" << status;
        }
        for (int i=0; i< num_params; i++) IaParams.x[i]=gsl_vector_get(solver->x ,i);
        gsl_vector_free(x);
        gsl_vector_free(steps);
        gsl_multimin_fminimizer_free (solver);
        if (IaParams.penfn!=0 && n>6) {
            //----------------------------------------
            //Minimize Is errors
            num_params=6;
            //init starting point
            gsl_vector * x = gsl_vector_alloc(num_params);
            gsl_vector_set_all(x,1.0);
            //init step size
            gsl_vector * steps = gsl_vector_alloc(num_params);
            gsl_vector_set_all(steps, 0.01);
            f.f = (double (* ) (const gsl_vector * x, void * params)) &kb_optimize::IsCalcErr;
            f.n = num_params;
            f.params = (void *) &IsParams;
            T = gsl_multimin_fminimizer_nmsimplex;
            solver = NULL;
            solver = gsl_multimin_fminimizer_alloc(T,num_params);
            gsl_multimin_fminimizer_set(solver, &f, x, steps);
            iter=0;
            qDebug() << "Optimizing Is";
            do {
                  iter++;
                  status = gsl_multimin_fminimizer_iterate(solver);
                  if (status) break;
                  size = gsl_multimin_fminimizer_size(solver);
                  status = gsl_multimin_test_size(size, 1e-4);
                  /* qDebug() <<"trying"
                  << "I0" << gsl_vector_get(solver->x, 0) * IsParams.I0
                  << "s" << gsl_vector_get(solver->x, 1) * IsParams.s
                  << "Up" << gsl_vector_get(solver->x, 2) * IsParams.mup
                  << "Us" << gsl_vector_get(solver->x, 3) * IsParams.mus
                  << "Ug" << gsl_vector_get(solver->x, 4) * IsParams.mug
                  << "ex" << gsl_vector_get(solver->x, 5) * IsParams.ex; */
                  //qDebug() <<"iter" << iter << "fval=" << solver->fval << "size=" << size;
            } while (status == GSL_CONTINUE && iter < max_iter);
            /*
            if (status == GSL_SUCCESS) {
                qDebug() <<"converged to minimum at"
                << "I0" << gsl_vector_get(solver->x, 0) * IsParams.I0
                << "s" << gsl_vector_get(solver->x, 1) * IsParams.s
                << "Up" << gsl_vector_get(solver->x, 2) * IsParams.mup
                << "Us" << gsl_vector_get(solver->x, 3) * IsParams.mus
                << "Ug" << gsl_vector_get(solver->x, 4) * IsParams.mug
                << "ex" << gsl_vector_get(solver->x, 5) * IsParams.ex;
            }
            else {
                qDebug() <<"converge failed at iteration " << iter
                << "I0" << gsl_vector_get(solver->x, 0) * IsParams.I0
                << "s" << gsl_vector_get(solver->x, 1) * IsParams.s
                << "Up" << gsl_vector_get(solver->x, 2) * IsParams.mup
                << "Us" << gsl_vector_get(solver->x, 3) * IsParams.mus
                << "Ug" << gsl_vector_get(solver->x, 4) * IsParams.mug
                << "ex" << gsl_vector_get(solver->x, 5) * IsParams.ex;
            }
            */
            for (int i=0; i< num_params; i++) IsParams.x[i]=gsl_vector_get(solver->x ,i);
            gsl_vector_free(x);
            gsl_vector_free(steps);
            gsl_multimin_fminimizer_free(solver);
         }
    }
    if ( n>6 ) {
        //find gm using model
        IaParams_t optParam;
        optParam.penfn = IaParams.penfn;
        optParam.vct = IaParams.vct;
        optParam.mu = IaParams.mu*IaParams.x[0];
        optParam.ex = IaParams.ex*IaParams.x[1];
        optParam.kg1 = IaParams.kg1*IaParams.x[2];
        optParam.kp = IaParams.kp*IaParams.x[3];
        optParam.kvb = IaParams.kvb*fabs(IaParams.x[4]);
        optParam.gp = IaParams.gp*IaParams.x[5];

        for (int i=0; i < n; i++) {
            float Ia =  IaParams.data->at(i).IaMod;
            float dIa = IaCalc(&optParam, IaParams.data->at(i).Va, IaParams.data->at(i).Vs, IaParams.data->at(i).Vg-0.1);
/*            qDebug() << "gm: Ia=" << Ia
                     << "dIa=" << dIa
                     << "Va="  <<  IaParams.data->at(i).Va
                     << "Vs="  <<  IaParams.data->at(i).Vs
                     << "Vg="  <<  IaParams.data->at(i).Vg-0.1
                     << "mu" << optParam.mu
                     << "ex" << optParam.ex
                     << "kg1" << optParam.kg1
                     << "kp" << optParam.kp
                     << "kvb" << optParam.kvb
                     << "gp" << optParam.gp
                     << "gm" << (Ia-dIa)/0.1; */
            //Update data table
            results_t r = IaParams.data->at(i);
            r.gm = (Ia-dIa)/0.1;
            IaParams.data->replace(i,r);
        }
    }
    else {
        for (int i=0; i < n; i++) {
            //Update data table
            results_t r = IaParams.data->at(i);
            r.gm = 0;
            IaParams.data->replace(i,r);
        }
    }
}

/*Plate current model
    IaCalc calculates vacuum tube plate current in Amperes.
    penfun =0 triode; 1-3 pentode

    Orignal matlab (c) Norman Koren, used here with kind permission.
    http://www.normankoren.com
    Many of the comments are retained from the original
    Matlab script.

    The following parameters have changed from the 1996 model:
    kg1 = KG1^EX is used inside exponent for optimization.
    KVB for TRIODE (now has units of volts).
    8/25/2001: I'm returning them to the original model for compatibility.
*/
double kb_optimize::IaCalcErr(const gsl_vector * xv, void *p) {
    float Ia;
    // x(i) is multiplied by MU, EX, kg1, KP, KVB, KG2
    IaParams_t * param =  (IaParams_t *) p;
    IaParams_t optParam;
    optParam.weight = param->weight;
    optParam.penfn = param->penfn;
    optParam.vct = param->vct;
    optParam.mu  = param->mu  * gsl_vector_get(xv,0);
    optParam.ex  = param->ex  * gsl_vector_get(xv,1);
    optParam.kg1 = param->kg1 * gsl_vector_get(xv,2);
    optParam.kp  = param->kp  * gsl_vector_get(xv,3);
    optParam.kvb = param->kvb * gsl_vector_get(xv,4);
    optParam.gp  = param->gp * gsl_vector_get(xv,5);
    if (param->penfn==0) optParam.kvb = fabs(optParam.kvb); //optimize on |x[4]| for a triode
    int n = param->data->length();
    float sq_err =0,err;
    for(int i=0; i < n; i++) {
        Ia = IaCalc(&optParam, param->data->at(i).Va, param->data->at(i).Vs, param->data->at(i).Vg);
        //calculate error
        err = (Ia - param->data->at(i).Ia); // WEIGHT == 1 or other:  Weight difference in currents.
        if (param->weight == 2) err *= log(Ia)/param->data->at(i).Ia; //Logarithmic weighing (intermediate between 1,3)
        else if (param->weight == 3) err /= param->data->at(i).Ia; //Weight relative difference
        sq_err +=err*err;
        //Update data table
        results_t r = param->data->at(i);
        r.IaMod=Ia;
        param->data->replace(i,r);
    }
    //qDebug() << "Ia err=" << sqrt(sq_err);
    return(sqrt(sq_err));
}

float kb_optimize::IaCalc(IaParams_t * param, float Va, float Vs, float Vg) {
    float pi=3.14159, Ia, E1;
    if (param->penfn==0) {
      //0=>triode
      E1 = Va /param->kp * log(1 + exp(param->kp * (1/param->mu + (Vg + param->vct)/sqrt(param->kvb + Va*Va))));
      if (E1 >= 0 ) Ia = 2*powf(E1,param->ex)/param->kg1; else Ia =0;
    }
    else {
        E1 = Vs/param->kp * log(1+exp(param->kp *(1/param->mu+(Vg + param->vct)/Vs)));
          if (E1 >= 0) Ia = (1+param->gp*Va)*2*powf(E1,param->ex)/param->kg1; else Ia = 0;
          if (param->penfn == 2)  Ia *= pi/2*(1-exp(-2 * Va/pi/param->kvb)); // intermediate.
          else if (param->penfn == 3) {
              Ia *= tanh(Va/param->kvb);
          }	// sharpest.
          else if (param->penfn == 4) {
//              float a = Va < param->kvb ? Va/param->kvb : 1;
//              Ia *= (1+cos(pi*(a-1)))/2;
//              Ia += Va/200000;
              Ia *= (1+cos(pi*(tanh(powf(Va/param->kvb,2))-1)))/2;
          }	// sharpest.
          else Ia *= atan(Va/param->kvb);	// (param->penfn == 1)atan(Vp/kvb) roundest. In article.
    }
    //qDebug() << "Opt IaCalc:Va, Vs, Vg, E1, E1', Ia=" << Va << Vs << Vg << E1 << (param->kp *(1/param->mu+(Vg + param->vct)/Vs)) << Ia*1000;
    return(Ia*1000); //convert to mA
}

// ======================================================================
// Screen current model
// Nick Barton http://www.bmamps.com
// Uses a model for the total current and then subtracts out the modeled
// anode current, in mA
double kb_optimize::IsCalcErr(const gsl_vector * x, void *p) {
    float sq_err=0;
    float err, Is, Ix;
    IsParams_t * param;
    param = (IsParams_t *)p;
    IsParams_t optParam;
    optParam.weight = param->weight;
    optParam.penfn = param->penfn;
    optParam.I0 = gsl_vector_get(x,0) * param->I0; // current fraction at zero plate voltage
    optParam.s =  gsl_vector_get(x,1) * param->s; // Sharpness
    optParam.mup = gsl_vector_get(x,2) * param->mup; // Up Plate Mu
    optParam.mus = gsl_vector_get(x,3) * param->mus; // Us Screen Mu
    optParam.mug = gsl_vector_get(x,4) * param->mug; // Ug Grid Mu
    optParam.ex  = gsl_vector_get(x,5) * param->ex;  // exponent
    for(int i=0; i < param->data->length(); i++) {
        results_t r = param->data->at(i);
        Ix = IsCalc(&optParam, r.Va, r.Vs, r.Vg);
        Is = Ix- r.IaMod; // Space current less Plate current leaves screen current
        err = Is - r.Is;
        //qDebug() << Ix << Is <<  r.IaMod << r.Va << r.Vs << r.Vg;
        if (Is>=0) r.IsMod =Is; else r.IsMod =0;
        //calculate error WEIGHT == 1 or other:  Weight difference in currents.
        //Logarithmic weighing (intermediate between 1,3)
        if (param->weight == 2) err *= log(Is)/param->data->at(i).Is;
        //Weight relative difference
        else if (param->weight == 3) err /= param->data->at(i).Is;
        param->data->replace(i,r);
        sq_err += err*err;
    }
    //qDebug() << "IsCalc: Is_err=" << sq_err;
    return(sqrt(sq_err));
}

float kb_optimize::IsCalc(IsParams_t * param, float Va, float Vs, float Vg) {
    // Space Charge model:
    float Ix = (param->mup*Va + param->mus*Vs + param->mug*Vg);
    if (Ix < 0) Ix=0;
    Ix = powf(fabs(Ix),param->ex);
    Ix *= (param->I0 +(1-param->I0)*2/3.14159*atan(param->s * Va/Vs));
    return(Ix);
}


void kb_optimize::Save_Spice_Model(QWidget * widget, QByteArray tnameba)
{
    char * tname = tnameba.data() ;
    static QString oldFileName;
    qDebug() << "Save path:" << oldFileName;
    if (oldFileName.length()==0) oldFileName=QDir::homePath();
    QString dataFileName = QFileDialog::getSaveFileName(widget, QObject::tr("Save Spice .cir file"), oldFileName,
      "Circuit files (*.cir);;Text files (*.txt);;All files (*.*)",
      new QString("Circuit files (*.cir)"));
    if (dataFileName!="") {
        int fs= dataFileName.lastIndexOf("/");
        oldFileName = dataFileName.mid(0,fs);
        //Save the model
        if (IaParams.data->length()>0) {
            QFile datafile(dataFileName);
            if (datafile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                char m[256];
                //Info
                sprintf(m,"* Triode and pentode plate currect models (c) Norman Koren http://www.normankoren.com, used with permission\n");
                sprintf(m,"* Open source Pentode screen currect model Nick Barton http://www.bmamps.com\n");
                sprintf(m,"* n");
                datafile.write(m);
                sprintf(m,"* library format: LTSpice\n");
                datafile.write(m);
                //Header
                //float mu,ex,kg1,kp,kvb;
                //float I0, s, mup, mus, mug, ex;
                switch (IaParams.penfn) {
                    case 0:
                    {
                    sprintf(m,".SUBCKT %s 1 2 3 ; P G C (Triode) ; TRIODE SECTION\n",tname);
                        datafile.write(m);
                        sprintf(m," X1 1 2 3 TRIODE MU=%.2f EX=%.2f KG1=%.1f KP=%.2f KVB=%.1f VCT=%.2f RGI=2000 CCG=%sp CPG1=%sp CCP=%sp ;\n",
                            IaParams.mu*IaParams.x[0],
                            IaParams.ex*IaParams.x[1],
                            IaParams.kg1*IaParams.x[2],
                            IaParams.kp*IaParams.x[3],
                            IaParams.kvb*fabs(IaParams.x[4]),
                            IaParams.vct,
                            "0","0","0");
                        datafile.write(m);
                        break;
                    }
                    case 1:
                    {
                    sprintf(m,".SUBCKT %s 1 2 3 4 ; P S G C (Pentode) ; PENTODE SECTION\n",tname);
                        datafile.write(m);
                        sprintf(m," X1 1 2 3 4 PENTODE1a MU=%.2f EX=%.2f KG1=%.1f KP=%.2f KVB=%.1f  GP=%f VCT=%.2f RGI=2000 CCG=%sp CPG1=%sp CCP=%sp \n",
                            IaParams.mu*IaParams.x[0],
                            IaParams.ex*IaParams.x[1],
                            IaParams.kg1*IaParams.x[2],
                            IaParams.kp*IaParams.x[3],
                            IaParams.kvb*IaParams.x[4],
                            IaParams.gp*IaParams.x[5],
                            IaParams.vct,
                            "0","0","0");
                        datafile.write(m);
                        sprintf(m," +I0=%.3f S=%.2f Up=%.6f Us=%.6f Ug=%.6f es=%.3f;\n",
                                IsParams.I0*IsParams.x[0],
                                IsParams.s*IsParams.x[1],
                                IsParams.mup*IsParams.x[2],
                                IsParams.mus*IsParams.x[3],
                                IsParams.mug*IsParams.x[4],
                                IsParams.ex*IsParams.x[5]);
                        datafile.write(m);
                        break;
                    }
                    case 2:
                    {
                    sprintf(m,".SUBCKT %s 1 2 3 4 ; P S G C (Pentode) ; PENTODE SECTION\n",tname);
                        datafile.write(m);
                        sprintf(m," X1 1 2 3 4 PENTODE2a MU=%.2f EX=%.2f KG1=%.1f KP=%.2f KVB=%.1f  GP=%f VCT=%.2f RGI=2000 CCG=%sp CPG1=%sp CCP=%sp \n",
                            IaParams.mu*IaParams.x[0],
                            IaParams.ex*IaParams.x[1],
                            IaParams.kg1*IaParams.x[2],
                            IaParams.kp*IaParams.x[3],
                            IaParams.kvb*IaParams.x[4],
                            IaParams.gp*IaParams.x[5],
                            IaParams.vct,
                            "0","0","0");
                        datafile.write(m);
                        sprintf(m," +I0=%.3f S=%.2f Up=%.6f Us=%.6f Ug=%.6f es=%.3f;\n",
                                IsParams.I0*IsParams.x[0],
                                IsParams.s*IsParams.x[1],
                                IsParams.mup*IsParams.x[2],
                                IsParams.mus*IsParams.x[3],
                                IsParams.mug*IsParams.x[4],
                                IsParams.ex*IsParams.x[5]);
                        datafile.write(m);
                        break;
                    }
                    case 3:
                    {
                    sprintf(m,".SUBCKT %s 1 2 3 4 ; P S G C (Pentode) ; PENTODE SECTION\n",tname);
                        datafile.write(m);
                        sprintf(m," X1 1 2 3 4 PENTODE3a MU=%.2f EX=%.2f KG1=%.1f KP=%.2f KVB=%.1f  GP=%f VCT=%.2f RGI=2000 CCG=%sp CPG1=%sp CCP=%sp \n",
                            IaParams.mu*IaParams.x[0],
                            IaParams.ex*IaParams.x[1],
                            IaParams.kg1*IaParams.x[2],
                            IaParams.kp*IaParams.x[3],
                            IaParams.kvb*IaParams.x[4],
                            IaParams.gp*IaParams.x[5],
                            IaParams.vct,
                            "0","0","0");
                        datafile.write(m);
                        sprintf(m," +I0=%.3f S=%.2f Up=%.6f Us=%.6f Ug=%.6f es=%.3f;\n",
                                IsParams.I0*IsParams.x[0],
                                IsParams.s*IsParams.x[1],
                                IsParams.mup*IsParams.x[2],
                                IsParams.mus*IsParams.x[3],
                                IsParams.mug*IsParams.x[4],
                                IsParams.ex*IsParams.x[5]);
                        datafile.write(m);
                        break;
                    }
                case 4:
                {
                sprintf(m,".SUBCKT %s 1 2 3 4 ; P S G C (Pentode) ; PENTODE SECTION\n",tname);
                    datafile.write(m);
                    sprintf(m," X1 1 2 3 4 PENTODE4a MU=%.2f EX=%.2f KG1=%.1f KP=%.2f KVB=%.1f  GP=%f VCT=%.2f RGI=2000 CCG=%sp CPG1=%sp CCP=%sp \n",
                        IaParams.mu*IaParams.x[0],
                        IaParams.ex*IaParams.x[1],
                        IaParams.kg1*IaParams.x[2],
                        IaParams.kp*IaParams.x[3],
                        IaParams.kvb*IaParams.x[4],
                        IaParams.gp*IaParams.x[5],
                        IaParams.vct,
                        "0","0","0");
                    datafile.write(m);
                    sprintf(m," +I0=%.3f S=%.2f Up=%.6f Us=%.6f Ug=%.6f es=%.3f;\n",
                            IsParams.I0*IsParams.x[0],
                            IsParams.s*IsParams.x[1],
                            IsParams.mup*IsParams.x[2],
                            IsParams.mus*IsParams.x[3],
                            IsParams.mug*IsParams.x[4],
                            IsParams.ex*IsParams.x[5]);
                    datafile.write(m);
                    break;
                }
                    default:
                        break;
                }
                sprintf(m, ".ENDS  %s\n",tname);
                datafile.write(m);
                datafile.close();
            }
        }
    }
}

