#-------------------------------------------------
#
# Project created by QtCreator 2016-10-31T02:08:48
#
#-------------------------------------------------

QT       += core gui
include(C:\Users\Akinori\Documents\Qt\qtpropertybrowser\src\qtpropertybrowser.pri)

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = onlineMotionCorrection
TEMPLATE = app

#INCLUDEPATH += C:\Users\Akinori\Downloads\opencv\build_binary\include
#LIBS += -LC:\Users\Akinori\Downloads\opencv\build_binary\x64\vc14\lib
INCLUDEPATH += C:\Users\Akinori\Downloads\opencv\build\install\include
LIBS += -LC:\Users\Akinori\Downloads\opencv\build\install\x64\vc12\lib

SOURCES += main.cpp\
        mainwindow.cpp \
    motioncorrectionthread.cpp \
    CV_SubPix.cpp \
    image_registrator.cpp \
    runguard.cpp \
    movingaverage.cpp

HEADERS  += mainwindow.h \
    motioncorrectionthread.h \
    mmap.h \
    CV_SubPix.h \
    image_registrator.h \
    runguard.h \
    movingaverage.h

FORMS    += mainwindow.ui

#CONFIG(release, debug|release) {
        DEFINES += QT_NO_DEBUG_OUTPUT
#}
