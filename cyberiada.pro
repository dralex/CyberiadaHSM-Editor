TARGET = CyberiadaHSME
QT += core gui widgets 
CONFIG += debug

SOURCES += \
	myassert.cpp \ 
	cyberiadasm_model.cpp \
	cyberiadasm_view.cpp \
	smeditor_window.cpp \
	main.cpp
HEADERS += \
	cyberiada_constants.h \
	myassert.h \
	cyberiadasm_model.h \
	cyberiadasm_view.h \
	smeditor_window.h \
	main.h
FORMS += smeditor_window.ui
RESOURCES += smeditor.qrc
LIBS += -Wl,--no-as-needed -L/usr/lib -lxml2 -L./cyberiadamlpp -lcyberiadamlpp -L./cyberiadaml -lcyberiadaml

INCLUDEPATH += cyberiadamlpp cyberiadaml
