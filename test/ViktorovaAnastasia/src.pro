QT       += core gui svg xml

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

win32-mingw{
    QMAKE_CXXFLAGS += -Wliteral-suffix
}
win32-msvc*{
    LIBS += -luser32
}

TARGET = VectorEditor
TEMPLATE = app
CONFIG += warn_on

DESTDIR = ../bin
MOC_DIR = ../build/moc
RCC_DIR = ../build/rcc
UI_DIR = ../build/ui
unix:OBJECTS_DIR = ../build/o/unix
win32:OBJECTS_DIR = ../build/o/win32
macx:OBJECTS_DIR = ../build/o/mac


SOURCES += main.cpp\
    mainwindow.cpp \
    svgreader.cpp \
    transition.cpp \
    verectangle.cpp \
    veworkplace.cpp \
    verectanglesettings.cpp \
    veselectionrect.cpp \
    colorlabel.cpp \
    vepolyline.cpp \
    vepolylinesettings.cpp \
    dotsignal.cpp \
    veworkplaceview.cpp

HEADERS  += mainwindow.h \
    svgreader.h \
    transition.h \
    verectangle.h \
    veworkplace.h \
    verectanglesettings.h \
    veselectionrect.h \
    colorlabel.h \
    vepolyline.h \
    vepolylinesettings.h \
    dotsignal.h \
    veworkplaceview.h

FORMS    += mainwindow.ui \
    verectanglesettings.ui \
    vepolylinesettings.ui

RESOURCES += \
    icons.qrc
