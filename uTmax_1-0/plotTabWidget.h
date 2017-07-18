#ifndef PLOTTABWIDGET_H
#define PLOTTABWIDGET_H

#include <QObject>
#include <QGroupBox>
#include "qcustomplot.h"
#include <QHBoxLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include <QPen>

#include <typedefs.h>

class PlotTabWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PlotTabWidget(QWidget * parent = 0);
    ~PlotTabWidget();
    void plotUpdate(plotInfo_t * plotinfo);

    void plot(plotInfo_t * plotInfo);
    void clearGraphs();
    QCustomPlot * plotArea;


signals:
    void penChanged();

public slots:
    void plotSettingChanged();

private slots:
    void PlotInteractionUser();
    void plotLegendChanged();

private:
    QComboBox * x1cb;
    QComboBox * y1cb;
    QComboBox * y2cb;
    QCheckBox * checkLegend;
    QCheckBox * checkPlotTriode;
    QSpinBox * CurveNum;
    QWidget * plotWidget;
    QLabel * cnLabel;
    QHBoxLayout * hl;
    QGroupBox *plotSetup;
    QVBoxLayout * vl;
    plotInfo_t plot1;
    void UpdateTitle(QString tube);
    QList<QPen> * ppenList=0;
    QCPPlotTitle * title = 0;

};

#endif // PLOTTABWIDGET_H
