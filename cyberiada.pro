TARGET = CyberiadaHSME
QT += core gui widgets 
CONFIG += debug

SOURCES += \
	myassert.cpp \ 
	cyberiada_abstract_item.cpp \
	cyberiada_visible_item.cpp \
	cyberiada_root_item.cpp \	
	cyberiadasm_item.cpp \	
	cyberiada_state_item.cpp \ 
	cyberiada_trans_item.cpp \
	cyberiadasm_model.cpp \
	cyberiadasm_tree_proxy_model.cpp \
	cyberiadasm_prop_proxy_model.cpp \
	cyberiadasm_view.cpp \
	cyberiadasm_prop_view.cpp \
	smeditor_window.cpp \
	main.cpp
HEADERS += \
	cyberiada_constants.h \
	myassert.h \
	cyberiada_abstract_item.h \
	cyberiada_root_item.h \
	cyberiada_visible_item.h \	
	cyberiadasm_item.h \
	cyberiada_state_item.h \
	cyberiada_trans_item.h \
	cyberiadasm_model.h \
	cyberiadasm_tree_proxy_model.h \
	cyberiadasm_prop_proxy_model.h \
	cyberiadasm_view.h \
	cyberiadasm_prop_view.h \
	smeditor_window.h \
	main.h
FORMS += smeditor_window.ui
RESOURCES += smeditor.qrc
LIBS += -Wl,--no-as-needed -L/usr/lib -lxml2 -L./cyberiadaml -lcyberiadaml

INCLUDEPATH += cyberiadaml
