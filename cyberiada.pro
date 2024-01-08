TARGET = CyberiadaHSME
QT += core gui widgets 
CONFIG += debug

SOURCES += \
	cyberiada_abstract_item.cpp \
	cyberiada_root_item.cpp \
	cyberiada_trans_item.cpp \
	cyberiadasm_item.cpp \
	cyberiadasm_model.cpp \	
	myassert.cpp \		      
	smeditor_window.cpp \
	cyberiadasm_view.cpp \
	main.cpp
HEADERS += cyberiada_constants.h \
	cyberiada_abstract_item.h \
	cyberiada_root_item.h \
	cyberiada_visible_item.h \	
	cyberiadasm_item.h \
	cyberiada_state_item.h \
	cyberiada_trans_item.h \
	myassert.h \
	cyberiadasm_model.h \
	smeditor_window.h \
	cyberiadasm_view.h \
	main.h
FORMS += smeditor_window.ui
RESOURCES += smeditor.qrc
LIBS += -Wl,--no-as-needed -L/usr/lib -lxml2 -L./cyberiadaml -lcyberiadaml

INCLUDEPATH += cyberiadaml
