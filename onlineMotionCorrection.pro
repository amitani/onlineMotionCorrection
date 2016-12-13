#-------------------------------------------------
#
# Project created by QtCreator 2016-10-31T02:08:48
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = onlineMotionCorrection
TEMPLATE = app

INCLUDEPATH += C:\opencv\build\install\include\
        C:\Users\Aki\Downloads\libtiff_lib\include
LIBS += -LC:\opencv\build\install\x64\vc14\lib

SOURCES += main.cpp\
        mainwindow.cpp \
    motioncorrectionthread.cpp \
    CV_SubPix.cpp \
    image_registrator.cpp \
    runguard.cpp

HEADERS  += mainwindow.h \
    motioncorrectionthread.h \
    mmap.h \
    CV_SubPix.h \
    image_registrator.h \
    CImg.h \
    runguard.h

FORMS    += mainwindow.ui


win32:LIBS += -lgdi32
win32:LIBS += -lUser32

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../../Downloads/libtiff_lib/lib/ -llibtiff
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../../Downloads/libtiff_lib/lib/ -llibtiffd

INCLUDEPATH += $$PWD/../../../Downloads/libtiff_lib/include
DEPENDPATH += $$PWD/../../../Downloads/libtiff_lib/include

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/../../../Downloads/libtiff_lib/lib/liblibtiff.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/../../../Downloads/libtiff_lib/lib/liblibtiffd.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/../../../Downloads/libtiff_lib/lib/libtiff.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/../../../Downloads/libtiff_lib/lib/libtiffd.lib



win32:CONFIG(release, debug|release): LIBS += c:/opencv/build/install/x64/vc14/lib/*310.lib
else:win32:CONFIG(debug, debug|release): LIBS += c:/opencv/build/install/x64/vc14/lib/*310d.lib

INCLUDEPATH += $$PWD/../../../../../opencv/build/install/include
DEPENDPATH += $$PWD/../../../../../opencv/build/install/include

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/../../../../../opencv/build/install/x64/vc14/lib/lib*310.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/../../../../../opencv/build/install/x64/vc14/lib/lib*310d.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/../../../../../opencv/build/install/x64/vc14/lib/*310.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/../../../../../opencv/build/install/x64/vc14/lib/*310d.lib
