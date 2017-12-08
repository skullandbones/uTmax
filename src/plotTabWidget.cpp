#include "plotTabWidget.h"
#include <QColor>
#include <QColorDialog>

PlotTabWidget::PlotTabWidget(QWidget *parent) :
    QWidget(parent)
{
    plot1.dataSet=0;
    x1cb = new QComboBox;
    x1cb->addItem("Va");
    x1cb->addItem("Vg2");
    x1cb->addItem("Vg");
    x1cb->setToolTip("<html><head/><body><p>Select the independant variable for the X axis</p></body></html>");
    connect(x1cb, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &PlotTabWidget::plotSettingChanged);

    y1cb = new QComboBox;
    y1cb->addItem("Ia(a)");
    y1cb->addItem("Ra(a)");
    y1cb->addItem("gm(a)");
    y1cb->addItem("mu(a)");
    y1cb->addItem("Ia(b)/g2");
    y1cb->addItem("Ra(b)");
    y1cb->addItem("gm(b)");
    y1cb->addItem("mu(b)");
    y1cb->setToolTip("<html><head/><body><p>Select the variable for the left Y axis</p></body></html>");
    connect(y1cb, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &PlotTabWidget::plotSettingChanged);

    y2cb = new QComboBox;
    y2cb->addItem("Off");
    y2cb->addItem("Ia(a) Model");
    y2cb->addItem("Ra(a)");
    y2cb->addItem("gm(a)");
    y2cb->addItem("mu(a)");
    y2cb->addItem("Ia(b)/g2");
    y2cb->addItem("Ia(b)/g2 Model");
    y2cb->addItem("Ra(b)");
    y2cb->addItem("gm(b)");
    y2cb->addItem("mu(b)");
    y2cb->setToolTip("<html><head/><body><p>Select the variable for the right Y axis</p></body></html>");
    connect(y2cb, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &PlotTabWidget::plotSettingChanged);

    checkLegend = new QCheckBox;
    checkLegend->setText("Legend");
    checkLegend->setToolTip("<html><head/><body><p>Display the plot ledgend</p></body></html>");

    connect(checkLegend, &QCheckBox::stateChanged,
            this, &PlotTabWidget::plotLegendChanged);

    checkPlotTriode = new QCheckBox;
    checkPlotTriode->setText("Triode Data");
    checkPlotTriode->setToolTip("<html><head/><body><p>Display only the data where Vg2=Va</p></body></html>");
    connect(checkPlotTriode, &QCheckBox::stateChanged,
            this, &PlotTabWidget::plotSettingChanged);

    CurveNum = new QSpinBox;
    CurveNum->setToolTip("<html><head/><body><p>Select the plot curve section for display. Zero selects all data.</p></body></html>");
    connect(CurveNum, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &PlotTabWidget::plotSettingChanged);

    cnLabel = new QLabel("Select Curve");

    hl = new QHBoxLayout;

    hl->setMargin(5);
    hl->addWidget(x1cb);
    hl->addWidget(y1cb);
    hl->addWidget(y2cb);
    hl->addWidget(checkLegend);
    hl->addWidget(checkPlotTriode);
    hl->addWidget(CurveNum);
    hl->addWidget(cnLabel);

    plotSetup = new QGroupBox(parent);
    plotSetup->setTitle("Plot Setup");
    plotSetup->setMaximumSize(641,51);
    plotSetup->setMinimumSize(641,51);
    plotSetup->setLayout(hl);

    plotWidget = new QWidget(parent);
    plotArea = new QCustomPlot(plotWidget);
    plotArea->setMaximumSize(641,421);
    plotArea->setMinimumSize(641,421);
    vl = new QVBoxLayout;
    vl->setMargin(0);
    vl->setAlignment(Qt::AlignHCenter);
    vl->addWidget(plotSetup);
    vl->addWidget(plotArea);
    //vl->setGeometry(QRect(1,1,641,421));
    this->setLayout(vl);

    //set the defaults
    y2cb->setCurrentIndex(0); //Off
    y1cb->setCurrentIndex(0);//default Ia
    x1cb->setCurrentIndex(0); //default Va
    checkLegend->setChecked(true);
    checkPlotTriode->setChecked(false);

    // Initialise plot Area
    //X axis title,scale
    plotArea->xAxis->setLabel("Va");
    plotArea->xAxis->setRange(0,100);
    //Y1 axis title, scale
    plotArea->yAxis->setLabel("Ia mA");
    plotArea->yAxis->setRange(0,1);
    //Y2 axis title, scale
    plotArea->yAxis2->setLabel("Ia/g2 mA");
    plotArea->yAxis2->setRange(0,1);
    plotArea->yAxis2->setVisible(false);
    plotArea->legend->setVisible(checkLegend->checkState());
    //Interactions
    plotArea->setInteraction(QCP::iSelectPlottables,true);
    plotArea->setInteraction(QCP::iRangeDrag,false);
    plotArea->setInteraction(QCP::iRangeZoom,false);
    connect(plotArea, SIGNAL(selectionChangedByUser()), this, SLOT(PlotInteractionUser()));

    //Title
    UpdateTitle("New");
//    plotArea->setRangeDrag(Qt::Vertical);
//    plotArea->setRangeZoom(Qt::Vertical);
}

PlotTabWidget::~PlotTabWidget()
{
    delete x1cb;
    delete y1cb;
    delete y2cb;
    delete checkLegend;
    delete checkPlotTriode;
    delete CurveNum;
    delete cnLabel;
    delete hl;
    delete plotSetup;
    delete plotWidget;
    delete plotArea;
    delete vl;
}


void PlotTabWidget::plotSettingChanged() {
    plotUpdate(&plot1);
}

void PlotTabWidget::plotUpdate(plotInfo_t *plotinfo) {
    plot1 = *plotinfo;
    float VaStart = plotinfo->VaStart;
    float VsStart = plotinfo->VsStart;
    float VgStart = plotinfo->VgStart;
    float VaEnd = plotinfo->VaEnd;
    float VsEnd = plotinfo->VsEnd;
    float VgEnd = plotinfo->VgEnd;
    int VaSteps = plotinfo->VaSteps;
    int VsSteps = plotinfo->VsSteps;
    int VgSteps = plotinfo->VgSteps;
    int curve = CurveNum->value();
    ppenList = plotinfo->penList;
    QList<results_t> * activeStore = plotinfo->dataSet;
    if (activeStore==0) return;
    if (activeStore->length()==0) return;
    int i;//The index of a data set in dataStore is Va_step + (Va_Steps+1)*VgStep + (Va_Steps+1)*(Vs_Steps+1)*VsStep
    int gr;
    QVector<double> x,y1,y2, y1f, y2f;
    QPen grPen;

    //adjust start position of data set for allow for triode section
    int first = 0, last = activeStore->length() > (VaSteps+1)*(VgSteps+1) ? (VaSteps+1)*(VgSteps+1) :activeStore->length();
    bool plotTri = true;

    if (plotinfo->type==NONE && !plotinfo->VsEQVa) {
        plotTri = false;
        last = activeStore->length();
    } else if ( (plotinfo->type==KOREN_P || plotinfo->type==DERK_P  || plotinfo->type==DERK_B || plotinfo->type==DERKE_P || plotinfo->type==DERKE_B)  && !checkPlotTriode->isChecked())
    {
        first = (VaSteps+1)*(VgSteps+1);
        last = activeStore->length();
        plotTri = false;
    }
    //limits
    float VaMax=0;
    float VsMax=0;
    float VgMin=0;
    float IaMax=0;
    float IsMax=0;
    float gmMax=0;
    float raMax=0;
    float muMax=0;
    for(int i=first; i < last; i++) {
        results_t dataPoint = activeStore->at(i);
        if (VaMax < dataPoint.Va) VaMax=dataPoint.Va;
        if (VsMax < dataPoint.Vs) VsMax=dataPoint.Vs;
        if (VgMin > dataPoint.Vg) VgMin=dataPoint.Vg;
        if (IaMax < dataPoint.Ia) IaMax=dataPoint.Ia;
        if (IsMax < dataPoint.Is) IsMax=dataPoint.Is;
        if (gmMax < dataPoint.gm_a) gmMax=dataPoint.gm_a;
        if (gmMax < dataPoint.gm_b) gmMax=dataPoint.gm_b;
        if (raMax < dataPoint.ra_a) raMax=dataPoint.ra_a;
        if (raMax < dataPoint.ra_b) raMax=dataPoint.ra_b;
        if (muMax < dataPoint.mu_a) muMax=dataPoint.mu_a;
        if (muMax < dataPoint.mu_b) muMax=dataPoint.mu_b;
    }

    //plot
    plotArea->clearGraphs();
    //plotArea->clearPlottables();

    if (activeStore->length()-first<2) {plotArea->replot(); return;}
    if (x1cb-> currentText()=="Va") {
        //X axis title,scale
        plotArea->xAxis->setLabel("Va");
        plotArea->xAxis->setRange(0,VaMax);
        //Build vectors
        for(int s=0,gr=0,gpnum=0; (s <= VsSteps) && (!plotTri || (s < 1)); s++) {
            if (s == (curve-1) || curve==0 || plotTri) {
                for(int g=0; (g <= VgSteps) ; g++) {
                    x.clear();y1.clear();y2.clear();
                    for(int a=0; (a <= VaSteps); a++) {
                        i = (VaSteps+1)*g + (VaSteps+1)*(VgSteps+1)*s + a + first;
                        if ((i) < activeStore->length()) {
                            results_t dataPoint = plotinfo->dataSet->at(i);
                            if (dataPoint.Ia!=-1) {
                                x.append(dataPoint.Va);
                                if (y1cb->currentText()=="Ia(a)") y1.append(dataPoint.Ia);
                                else if (y1cb->currentText()=="Ia(b)/g2") y1.append(dataPoint.Is);
                                else if (y1cb->currentText()=="Ra(a)") y1.append(dataPoint.ra_a);
                                else if (y1cb->currentText()=="Ra(b)") y1.append(dataPoint.ra_b);
                                else if (y1cb->currentText()=="gm(a)") y1.append(dataPoint.gm_a);
                                else if (y1cb->currentText()=="gm(b)") y1.append(dataPoint.gm_b);
                                else if (y1cb->currentText()=="mu(a)") y1.append(dataPoint.mu_a);
                                else if (y1cb->currentText()=="mu(b)") y1.append(dataPoint.mu_b);
                                if (y2cb->currentText()=="Ia(b)/g2") y2.append(dataPoint.Is);
                                else if (y2cb->currentText()=="Ia(a) Model") y2.append(dataPoint.IaMod);
                                else if (y2cb->currentText()=="Ia(b)/g2 Model") y2.append(dataPoint.IsMod);
                                else if (y2cb->currentText()=="Ra(a)") y2.append(dataPoint.ra_a);
                                else if (y2cb->currentText()=="Ra(b)") y2.append(dataPoint.ra_b);
                                else if (y2cb->currentText()=="gm(a)") y2.append(dataPoint.gm_a);
                                else if (y2cb->currentText()=="gm(b)") y2.append(dataPoint.gm_b);
                                else if (y2cb->currentText()=="mu(a)") y2.append(dataPoint.mu_a);
                                else if (y2cb->currentText()=="mu(b)") y2.append(dataPoint.mu_b);
                            }
                        }
                    }
                    plotArea->addGraph(plotArea->xAxis,plotArea->yAxis);
                    plotArea->graph(gr)->setData(x,y1);
                    //Are enough pens defined?
                    if (gpnum > (plotinfo->penList->length()-1)) {
                        //no pen so add one
                        plotinfo->penList->append(plotinfo->penList->at(gpnum % plotinfo->penList->length()));
                    }
                    plotArea->graph(gr)->setPen(plotinfo->penList->at(gpnum));
                    QString grlabel;
                    float Vs = VsStart;
                    if (VsSteps>=1) Vs += (float)s*(float)(VsEnd - VsStart)/(float)VsSteps;
                    float Vg = VgStart;
                    if (VgSteps>=1) Vg += (float)g*(float)(VgEnd - VgStart)/(float)VgSteps;
                    Vg = floor(0.5+10*Vg)/10;
                    Vs = floor(0.5+10*Vs)/10;
                    //TODO: check - used to use Iaparams.penfun
                    if ( plotinfo->type==DIODE || plotinfo->type==TRIODE || plotinfo->type==DUAL_TRIODE || plotTri ) QTextStream(&grlabel)  << "Vg1:" << Vg;
                    else QTextStream(&grlabel)  << "Vg1:" << Vg << " Vg2:" << Vs;
                    plotArea->graph(gr++)->setName(grlabel);
                    if (y2cb->currentText()!="Off") {
                        plotArea->addGraph(plotArea->xAxis,plotArea->yAxis2);
                        plotArea->graph(gr)->setData(x,y2);
                        grPen = plotinfo->penList->at(gpnum);
                        grPen.setStyle(Qt::DotLine);
                        plotArea->graph(gr)->setPen(grPen);
                        plotArea->graph(gr++)->setName(grlabel);
                    }
                    gpnum++;
                }
            }
        }
    }
    if (x1cb-> currentText()=="Vg2") {
        //X axis title,scale
        plotArea->xAxis->setLabel("Va/g2");
        plotArea->xAxis->setRange(0,VsMax);
        //Build vectors
        for(int a=0,gr=0,gpnum=0; (a <= VaSteps) ; a++) {
            if (a == (curve-1) || 0==curve  || plotTri) {
                for(int g=0; (g <= VgSteps) ; g++) {
                    x.clear();y1.clear();y2.clear();
                    for(int s=0; (s <= VsSteps)  && (!plotTri || (s < 1)); s++) {
                        i = a + (VaSteps+1)*g + (VaSteps+1)*(VgSteps+1)*s + first;
                        if (i < activeStore->length()) {
                            results_t dataPoint = activeStore->at(i);
                            if (dataPoint.Ia!=-1) {
                                x.append(dataPoint.Vs);
                                if (y1cb->currentText()=="Ia(a)") y1.append(dataPoint.Ia);
                                else if (y1cb->currentText()=="Ia(b)/g2") y1.append(dataPoint.Is);
                                else if (y1cb->currentText()=="Ra(a)") y1.append(dataPoint.ra_a);
                                else if (y1cb->currentText()=="Ra(b)") y1.append(dataPoint.ra_b);
                                else if (y1cb->currentText()=="gm(a)") y1.append(dataPoint.gm_a);
                                else if (y1cb->currentText()=="gm(b)") y1.append(dataPoint.gm_b);
                                else if (y1cb->currentText()=="mu(a)") y1.append(dataPoint.mu_a);
                                else if (y1cb->currentText()=="mu(b)") y1.append(dataPoint.mu_b);
                                if (y2cb->currentText()=="Ia(b)/g2") y2.append(dataPoint.Is);
                                else if (y2cb->currentText()=="Ia(a) Model") y2.append(dataPoint.IaMod);
                                else if (y2cb->currentText()=="Ia(b)/g2 Model") y2.append(dataPoint.IsMod);
                                else if (y2cb->currentText()=="Ra(a)") y2.append(dataPoint.ra_a);
                                else if (y2cb->currentText()=="Ra(b)") y2.append(dataPoint.ra_b);
                                else if (y2cb->currentText()=="gm(a)") y2.append(dataPoint.gm_a);
                                else if (y2cb->currentText()=="gm(b)") y2.append(dataPoint.gm_b);
                                else if (y2cb->currentText()=="mu(a)") y2.append(dataPoint.mu_a);
                                else if (y2cb->currentText()=="mu(b)") y2.append(dataPoint.mu_b);
                            }
                        }
                    }
                    plotArea->addGraph(plotArea->xAxis,plotArea->yAxis);
                    plotArea->graph(gr)->setData(x,y1);
                    //Are enough pens defined?
                    if (gpnum > (plotinfo->penList->length()-1)) {
                        //no pen so add one
                        plotinfo->penList->append(plotinfo->penList->at(gpnum % plotinfo->penList->length()));
                    }
                    plotArea->graph(gr)->setPen(plotinfo->penList->at(gpnum));
                    QString grlabel;
                    float Va = VaStart;
                    if (VaSteps>=1) Va += (float)a*(float)(VaEnd - VaStart)/(float)VaSteps;
                    float Vg = VgStart;
                    if (VgSteps>=1) Vg += (float)g*(float)(VgEnd - VgStart)/(float)VgSteps;
                    Vg = floor(0.5+10*Vg)/10;
                    Va = floor(0.5+10*Va)/10;
                    QTextStream(&grlabel)  << "Va:" << Va << " Vg:" << Vg;
                    plotArea->graph(gr++)->setName(grlabel);
                    if (y2cb->currentText()!="Off") {
                        plotArea->addGraph(plotArea->xAxis,plotArea->yAxis2);
                        plotArea->graph(gr)->setData(x,y2);
                        grPen = plotinfo->penList->at(gpnum);
                        grPen.setStyle(Qt::DotLine);
                        plotArea->graph(gr)->setPen(grPen);
                        plotArea->graph(gr++)->setName(grlabel);
                    }
                    gpnum++;
                }
            }
        }
    }
    if (x1cb-> currentText()=="Vg") {
        //X axis title,scale
        plotArea->xAxis->setLabel("Vg");
        plotArea->xAxis->setRange(VgMin,0);

        for(int s=0,gr=0,gpnum=0; (s <= VsSteps)  && (!plotTri || (s < 1)); s++) {
            if (s == (curve-1) || 0==curve || plotTri) {
                int astep = VaSteps/5;
                for(int a=0; (a <= VaSteps); a+=astep) {
                    x.clear();y1.clear();y2.clear();
                    for(int g=0; (g <= VgSteps); g++) {
                        i = a + (VaSteps+1)*g + (VaSteps+1)*(VgSteps+1)*s + first;
                        if (i <activeStore->length()) {
                            results_t dataPoint = activeStore->at(i);
                            if (dataPoint.Ia!=-1) {
                                x.append(dataPoint.Vg);
                                if (y1cb->currentText()=="Ia(a)") y1.append(dataPoint.Ia);
                                else if (y1cb->currentText()=="Ia(b)/g2") y1.append(dataPoint.Is);
                                else if (y1cb->currentText()=="Ra(a)") y1.append(dataPoint.ra_a);
                                else if (y1cb->currentText()=="Ra(b)") y1.append(dataPoint.ra_b);
                                else if (y1cb->currentText()=="gm(a)") y1.append(dataPoint.gm_a);
                                else if (y1cb->currentText()=="gm(b)") y1.append(dataPoint.gm_b);
                                else if (y1cb->currentText()=="mu(a)") y1.append(dataPoint.mu_a);
                                else if (y1cb->currentText()=="mu(b)") y1.append(dataPoint.mu_b);
                                if (y2cb->currentText()=="Ia(b)/g2") y2.append(dataPoint.Is);
                                else if (y2cb->currentText()=="Ia(a) Model") y2.append(dataPoint.IaMod);
                                else if (y2cb->currentText()=="Ia(b)/g2 Model") y2.append(dataPoint.IsMod);
                                else if (y2cb->currentText()=="Ra(a)") y2.append(dataPoint.ra_a);
                                else if (y2cb->currentText()=="Ra(b)") y2.append(dataPoint.ra_b);
                                else if (y2cb->currentText()=="gm(a)") y2.append(dataPoint.gm_a);
                                else if (y2cb->currentText()=="gm(b)") y2.append(dataPoint.gm_b);
                                else if (y2cb->currentText()=="mu(a)") y2.append(dataPoint.mu_a);
                                else if (y2cb->currentText()=="mu(b)") y2.append(dataPoint.mu_b);
                            }
                        }
                    }
                    plotArea->addGraph(plotArea->xAxis,plotArea->yAxis);
                    plotArea->graph(gr)->setData(x,y1);
                    //Are enough pens defined?
                    if (gpnum > (plotinfo->penList->length()-1)) {
                        //no pen so add one
                        plotinfo->penList->append(plotinfo->penList->at(gpnum % 16));
                    }
                    plotArea->graph(gr)->setPen(plotinfo->penList->at(gpnum));
                    QString grlabel;
                    float Vs = VsStart;
                    if (VsSteps>=1) Vs += (float)s*(float)(VsEnd - VsStart)/(float)VsSteps;
                    float Va = VaStart;
                    if (VaSteps>=1) Va += (float)a*(float)(VaEnd - VaStart)/(float)VaSteps;
                    Va = floor(0.5+10*Va)/10;
                    Vs = floor(0.5+10*Vs)/10;
                    if ( plotinfo->type==DIODE || plotinfo->type==TRIODE || plotinfo->type==DUAL_TRIODE || plotTri)
                        QTextStream(&grlabel)  << "Va:" << Va;
                    else
                        QTextStream(&grlabel)  << "Va:" << Va << " Vg2:" << Vs;
                    plotArea->graph(gr++)->setName(grlabel);
                    if (y2cb->currentText()!="Y2=Off") {
                        plotArea->addGraph(plotArea->xAxis,plotArea->yAxis2);
                        plotArea->graph(gr)->setData(x,y2);
                        grPen = plotinfo->penList->at(gpnum);
                        grPen.setStyle(Qt::DotLine);
                        plotArea->graph(gr)->setPen(grPen);
                        plotArea->graph(gr++)->setName(grlabel);
                    }
                    gpnum++;
                }
            }
        }
    }
    //Axis titles, scale
    plotArea->yAxis->setLabel("n/a");
    plotArea->yAxis2->setLabel("n/a");
    if (plotinfo->type==DIODE) {
        if (y1cb->currentText()=="Ia(a)") plotArea->yAxis->setLabel("Ia mA");
        if (y1cb->currentText()=="Ra(a)") plotArea->yAxis->setLabel("Ra kOhm");
        if (y2cb->currentText()=="Ia(a)") plotArea->yAxis2->setLabel("Ia mA");
        if (y2cb->currentText()=="Ia(a) Model") plotArea->yAxis2->setLabel("Ia Model mA");
        if (y2cb->currentText()=="Ra(a)") plotArea->yAxis2->setLabel("Ra kOhm");
    } else if (plotinfo->type==DUAL_DIODE) {
        if (y1cb->currentText()=="Ia(a)") plotArea->yAxis->setLabel("Ia(a) mA");
        if (y1cb->currentText()=="Ra(a)") plotArea->yAxis->setLabel("Ra(a) kOhm");
        if (y1cb->currentText()=="Ra(b)") plotArea->yAxis->setLabel("Ra(b) kOhm");
        if (y2cb->currentText()=="Ia(a)") plotArea->yAxis2->setLabel("Ia(a) mA");
        if (y2cb->currentText()=="Ia(b)/g2") plotArea->yAxis2->setLabel("Ia(b) mA");
        if (y2cb->currentText()=="Ia(a) Model") plotArea->yAxis2->setLabel("Ia(a) Model mA");
        if (y2cb->currentText()=="Ia(b)/g2 Model") plotArea->yAxis2->setLabel("Ia(b) Model mA");
        if (y2cb->currentText()=="Ra(a)") plotArea->yAxis2->setLabel("Ra(a) kOhm");
        if (y2cb->currentText()=="Ra(b)") plotArea->yAxis2->setLabel("Ra(b) kOhm");
    } else if (plotinfo->type==TRIODE ) {
        if (y1cb->currentText()=="Ia(a)") plotArea->yAxis->setLabel("Ia mA");
        if (y1cb->currentText()=="Ra(a)") plotArea->yAxis->setLabel("Ra kOhm");
        if (y1cb->currentText()=="gm(a)") plotArea->yAxis->setLabel("-Gm mS");
        if (y1cb->currentText()=="mu(a)") plotArea->yAxis->setLabel("-mu");
        if (y2cb->currentText()=="Ia(a)") plotArea->yAxis2->setLabel("Ia mA");
        if (y2cb->currentText()=="Ia(a) Model") plotArea->yAxis2->setLabel("Ia Model mA");
        if (y2cb->currentText()=="Ra(a)") plotArea->yAxis2->setLabel("Ra kOhm");
        if (y2cb->currentText()=="gm(a)") plotArea->yAxis2->setLabel("-Gm mS");
        if (y2cb->currentText()=="mu(a)") plotArea->yAxis2->setLabel("-mu");
    } else if (plotinfo->type==DUAL_TRIODE ) {
        //a
        if (y1cb->currentText()=="Ia(a)") plotArea->yAxis->setLabel("Ia mA");
        if (y1cb->currentText()=="Ra(a)") plotArea->yAxis->setLabel("Ra kOhm");
        if (y1cb->currentText()=="gm(a)") plotArea->yAxis->setLabel("-Gm mS");
        if (y1cb->currentText()=="mu(a)") plotArea->yAxis->setLabel("-mu");
        if (y1cb->currentText()=="Ia(b)/g2") plotArea->yAxis->setLabel("Ia(b) mA");
        if (y2cb->currentText()=="Ia(a)") plotArea->yAxis2->setLabel("Ia mA");
        if (y2cb->currentText()=="Ia(a) Model") plotArea->yAxis2->setLabel("Ia Model mA");
        if (y2cb->currentText()=="Ra(a)") plotArea->yAxis2->setLabel("Ra kOhm");
        if (y2cb->currentText()=="gm(a)") plotArea->yAxis2->setLabel("-Gm mS");
        if (y2cb->currentText()=="mu(a)") plotArea->yAxis2->setLabel("-mu");
        //b
        if (y1cb->currentText()=="Ia(b)") plotArea->yAxis->setLabel("Ia(b) mA");
        if (y1cb->currentText()=="Ra(b)") plotArea->yAxis->setLabel("Ra(b) kOhm");
        if (y1cb->currentText()=="gm(b)") plotArea->yAxis->setLabel("-Gm(b) mS");
        if (y1cb->currentText()=="mu(b)") plotArea->yAxis->setLabel("-mu(b)");
        if (y2cb->currentText()=="Ia(b)/g2") plotArea->yAxis2->setLabel("Ia(b) mA");
        if (y2cb->currentText()=="Ia(b) Model") plotArea->yAxis2->setLabel("Ia(b) Model mA");
        if (y2cb->currentText()=="Ra(b)") plotArea->yAxis2->setLabel("Ra(b) kOhm");
        if (y2cb->currentText()=="gm(b)") plotArea->yAxis2->setLabel("-Gm(b) mS");
        if (y2cb->currentText()=="mu(b)") plotArea->yAxis2->setLabel("-mu(b)");
    } else {
        if (y1cb->currentText()=="Ia(a)") plotArea->yAxis->setLabel("Ia mA");
        if (y1cb->currentText()=="Ia(b)/g2") plotArea->yAxis->setLabel("Ig2 mA");
        if (y1cb->currentText()=="Ra(a)") plotArea->yAxis->setLabel("Ra kOhm");
        if (y1cb->currentText()=="gm(a)") plotArea->yAxis->setLabel("-Gm mS");
        if (y1cb->currentText()=="mu(a)") plotArea->yAxis->setLabel("-mu");
        if (y2cb->currentText()=="Ia(a)") plotArea->yAxis2->setLabel("Ia mA");
        if (y2cb->currentText()=="Ia(b)/g2") plotArea->yAxis2->setLabel("g2 mA");
        if (y2cb->currentText()=="Ia(a) Model") plotArea->yAxis2->setLabel("Ia Model mA");
        if (y2cb->currentText()=="Ia(b)/g2 Model") plotArea->yAxis2->setLabel("Ig2 Model mA");
        if (y2cb->currentText()=="Ra(a)") plotArea->yAxis2->setLabel("Ra kOhm");
        if (y2cb->currentText()=="gm(a)") plotArea->yAxis2->setLabel("-Gm mS");
        if (y2cb->currentText()=="mu(a)") plotArea->yAxis2->setLabel("-mu");
    }

    if (y2cb->currentText()=="Off") plotArea->yAxis2->setVisible(false);
    else plotArea->yAxis2->setVisible(true);
    UpdateTitle(plotinfo->tube);
    if (y2cb->currentText()!="Off" ) {
        int c =  plotArea->legend->itemCount()/2;
        for (int i=1; i <= c; i++) plotArea->legend->removeItem(i);
    }
    plotArea->axisRect()->insetLayout()->setInsetAlignment(0,Qt::AlignLeft|Qt::AlignTop);
//    plotArea->legend->setPositionStyle(QCPLegend::psTopLeft);
    plotArea->legend->setLayer("grid");
    plotArea->legend->setVisible(checkLegend->checkState());
//    plotArea->legend->autoSize();
    //plotArea->axisRect()->insetLayout()->;
    //Rescale axis to match
    //qDebug() << "RePlot: IaMax=" << IaMax << "IsMax=" << IaMax;
    IaMax= ceil(IaMax);
    IsMax= ceil(IsMax);
    if (y1cb->currentText()=="Ia(a)") {
        plotArea->yAxis->setRange(0,IaMax);
        plotArea->yAxis2->setRange(0,IaMax);
        if (y2cb->currentText()=="gm(a)") plotArea->yAxis2->setRange(0,gmMax);
        if (y2cb->currentText()=="Ra(a)") plotArea->yAxis2->setRange(0,raMax);
        if (y2cb->currentText()=="mu(a)") plotArea->yAxis2->setRange(0,muMax);
        if (y2cb->currentText()=="gm(b)") plotArea->yAxis2->setRange(0,gmMax);
        if (y2cb->currentText()=="Ra(b)") plotArea->yAxis2->setRange(0,raMax);
        if (y2cb->currentText()=="mu(b)") plotArea->yAxis2->setRange(0,muMax);
    }
    else if (y1cb->currentText()=="Ia(b)/g2") {
        plotArea->yAxis->setRange(0,IsMax);
        plotArea->yAxis2->setRange(0,IsMax);
        if (y2cb->currentText()=="Ia(b) Model") {
            plotArea->yAxis->setRange(0,IaMax);
            plotArea->yAxis2->setRange(0,IaMax);
        }
        if (y2cb->currentText()=="gm(a)") plotArea->yAxis2->setRange(0,gmMax);
        if (y2cb->currentText()=="Ra(a)") plotArea->yAxis2->setRange(0,raMax);
        if (y2cb->currentText()=="mu(a)") plotArea->yAxis2->setRange(0,muMax);
        if (y2cb->currentText()=="gm(b)") plotArea->yAxis2->setRange(0,gmMax);
        if (y2cb->currentText()=="Ra(b)") plotArea->yAxis2->setRange(0,raMax);
        if (y2cb->currentText()=="mu(b)") plotArea->yAxis2->setRange(0,muMax);
    }
    else if ( (y1cb->currentText()=="gm(a)") || (y1cb->currentText()=="gm(b)") ) {
        plotArea->yAxis->setRange(0,gmMax);
        if (y2cb->currentText()=="Ia(b)/g2") plotArea->yAxis2->setRange(0,IsMax);
        if (y2cb->currentText()=="Ia(a) Model") plotArea->yAxis2->setRange(0,IaMax);
        if (y2cb->currentText()=="Ia(b)/g2 Model") plotArea->yAxis2->setRange(0,IsMax);
        if (y2cb->currentText()=="gm(a)") plotArea->yAxis2->setRange(0,gmMax);
        if (y2cb->currentText()=="Ra(a)") plotArea->yAxis2->setRange(0,raMax);
        if (y2cb->currentText()=="mu(a)") plotArea->yAxis2->setRange(0,muMax);
        if (y2cb->currentText()=="gm(b)") plotArea->yAxis2->setRange(0,gmMax);
        if (y2cb->currentText()=="Ra(b)") plotArea->yAxis2->setRange(0,raMax);
        if (y2cb->currentText()=="mu(b)") plotArea->yAxis2->setRange(0,muMax);
    }
    else if ( (y1cb->currentText()=="Ra(a)") || (y1cb->currentText()=="Ra(b)") ) {
        plotArea->yAxis->setRange(0,raMax);
        if (y2cb->currentText()=="Ia(b)/g2") plotArea->yAxis2->setRange(0,IsMax);
        if (y2cb->currentText()=="Ia(a) Model") plotArea->yAxis2->setRange(0,IaMax);
        if (y2cb->currentText()=="Ia(b)/g2 Model") plotArea->yAxis2->setRange(0,IsMax);
        if (y2cb->currentText()=="gm(a)") plotArea->yAxis2->setRange(0,gmMax);
        if (y2cb->currentText()=="Ra(a)") plotArea->yAxis2->setRange(0,raMax);
        if (y2cb->currentText()=="mu(a)") plotArea->yAxis2->setRange(0,muMax);
        if (y2cb->currentText()=="gm(b)") plotArea->yAxis2->setRange(0,gmMax);
        if (y2cb->currentText()=="Ra(b)") plotArea->yAxis2->setRange(0,raMax);
        if (y2cb->currentText()=="mu(b)") plotArea->yAxis2->setRange(0,muMax);
    }
    else if ( (y1cb->currentText()=="mu(a)") || (y1cb->currentText()=="mu(b)") ) {
        plotArea->yAxis->setRange(0,muMax);
        if (y2cb->currentText()=="Ia(b)/g2") plotArea->yAxis2->setRange(0,IsMax);
        if (y2cb->currentText()=="Ia(a) Model") plotArea->yAxis2->setRange(0,IaMax);
        if (y2cb->currentText()=="Ia(b)/g2 Model") plotArea->yAxis2->setRange(0,IsMax);
        if (y2cb->currentText()=="gm(a)") plotArea->yAxis2->setRange(0,gmMax);
        if (y2cb->currentText()=="Ra(a)") plotArea->yAxis2->setRange(0,raMax);
        if (y2cb->currentText()=="mu(a)") plotArea->yAxis2->setRange(0,muMax);
        if (y2cb->currentText()=="gm(b)") plotArea->yAxis2->setRange(0,gmMax);
        if (y2cb->currentText()=="Ra(b)") plotArea->yAxis2->setRange(0,raMax);
        if (y2cb->currentText()=="mu(b)") plotArea->yAxis2->setRange(0,muMax);
    }

    //plotArea->rescaleAxes();
    plotArea->replot();
}

void PlotTabWidget::UpdateTitle(QString tube) {
    QString line;
    line.append(tube);
    /*line +=" ";
    if (y1cb->currentText()=="Ia(a)") line += "Ia";
    if (y1cb->currentText()=="Ia(b)/g2") line += "Ia/g2";
    if (y1cb->currentText()=="gm(a)") line += "gm(a)";
    if (y1cb->currentText()=="gm(b)") line += "gm(b)";
    if (y2cb->currentText()=="Ia(a) Model") line += ", Ia(a) Model";
    if (y2cb->currentText()=="Ia(b)/g2") line += ", Ia/g2";
    if (y2cb->currentText()=="Ia(b)/g2 Model") line += ", Ia(b)/g2 Model";
    if (y2cb->currentText()=="gm(a)") line += ", gm(a)";
    if (y2cb->currentText()=="gm(b)") line += ", gm(b)";
    if (x1cb->currentText()=="Va(a)") line.append("/Va1");
    if (x1cb->currentText()=="Vg2") line.append("/Vg2");
    if (x1cb->currentText()=="Vg") line.append("/Vg1");*/
    //plotArea->setTitle(line);
    if (title == 0) {
        title = new QCPPlotTitle(plotArea, line);
        plotArea->plotLayout()->insertRow(0);
        plotArea->plotLayout()->addElement(0, 0, title);
    }
    else {
      title->setText(line);
    }

    plotArea->replot();
}

//------------------------------
// Handle Plot Interactions
//------------------------------
void PlotTabWidget::PlotInteractionUser() {

    if (ppenList==0) return;
    QList<QCPGraph *> line = plotArea->selectedGraphs();
    if (line.length()>0) {
        //Determine the index of the selected graph
        int index=0;
        while ( plotArea->graph(index) != line.at(0) && index < plotArea->graphCount() ) {
            index++;
        }
        QColor lineColor = line.at(0)->pen().color();
        QColor c = QColorDialog::getColor(lineColor,this,"Select New Line Color");
        if (c.isValid()) {
            QPen grPen;
            grPen.setColor(c);
            grPen.setWidthF(2);
            if (y2cb->currentText() != "Off") {
                //plotArea->graph(index)->setPen(grPen);
                index >>= 1;
                ppenList->replace(index,grPen);
            } else {
                //line.at(0)->setPen(grPen);
                ppenList->replace(index,grPen);
            }
        }
        plotArea->deselectAll();
        plotUpdate(&plot1);
        //TODO Save the new scheme
        emit penChanged();
    }
}

void PlotTabWidget::clearGraphs() {
    plotArea->clearGraphs();
}

void PlotTabWidget::plotLegendChanged() {
    plotArea->legend->setVisible( checkLegend->isChecked() );
    plotArea->replot();
}

