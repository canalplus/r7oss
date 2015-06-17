TARGET = qlinuxinputplugin
#load(qt_plugin)
include(../../qpluginbase.pri)

DESTDIR = $$(D)/usr/plugins/generic
target.path = $$[QT_INSTALL_PLUGINS]/generic
INSTALLS += target

HEADERS	= qlinuxinput.h qkbd.h qkbd_p.h qkbd_defaultmap_p.h \
    qlinuxinputkeyboardwatcher.h

QT += core-private

LIBS += -ludev

SOURCES	= main.cpp \
	qlinuxinput.cpp \
	qkbd.cpp \
    qlinuxinputkeyboardwatcher.cpp

