
QT       += core gui widgets
greaterThan(QT_MAJOR_VERSION, 4) {
    QT += printsupport
}

TARGET = uTmax
TEMPLATE = app
RC_FILE = icon.rc

#CONFIG+= static
#CONFIG+= console
QT += serialport

CONFIG(debug, debug|release) {
    OBJECTS_DIR = debug_obj
    MOC_DIR = debug_obj
    DESTDIR = debug
}

CONFIG(release, debug|release) {
    OBJECTS_DIR = release_obj
    MOC_DIR = release_obj
    DESTDIR = release

}

# macx unix win32
!win32 {
# 	include(../3rdparty/qextserialport-1.2rc/src/qextserialport.pri)
	include(../3rdparty/gsl/gsl.pri)
}
win32 {
    INCLUDEPATH += ../3rdparty/gsl-1.8-lib/include
    LIBS += -L../3rdparty/gsl-1.8-bin/bin
    LIBS += -llibgsl
    LIBS += -llibgslcblas
}

RESOURCES += utMax.qrc

SOURCES += main.cpp\
        mainwindow.cpp \
    cal_dialog.cpp \
    debug_dialog.cpp \
    options_dialog.cpp \
    qcustomplot.cpp \
    datasavedialog.cpp \
    dr_optimize.cpp \
    plotTabWidget.cpp

HEADERS  += mainwindow.h \
    cal_dialog.h \
    debug_dialog.h \
    options_dialog.h \
    qcustomplot.h \
    datasavedialog.h \
    typedefs.h \
    dr_optimize.h \
    plotTabWidget.h

FORMS    +=  cal_dialog.ui \
    debug_dialog.ui \
    options_dialog.ui \
    datasavedialog.ui

#unix {
#        FORMS    += mainwindow_l.ui
#}
#macx {
#        FORMS    += mainwindow_osx.ui
#}
#win32 {
        FORMS    += mainwindow.ui
#}

DISTFILES += \
    Style.qss
