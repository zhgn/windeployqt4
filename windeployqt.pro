TEMPLATE = app
TARGET = windeployqt
DESTDIR = ../../bin
QT -= gui
QT += xmlpatterns

build_all:!build_pass {
    CONFIG -= build_all
    CONFIG += release
}

target.path = $$[QT_INSTALL_BINS]
INSTALLS += target

# This ensures we get stderr and stdout on Windows.
CONFIG += console

# This ensures that this is a command-line program on OS X and not a GUI application.
CONFIG -= app_bundle

#QT = core-private
DEFINES += QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII QT_NO_FOREACH

include(QJson/QJson.pri)
include(qt592/qt592.pri)

INCLUDEPATH += $$PWD
SOURCES += main.cpp utils.cpp qmlutils.cpp elfreader.cpp
HEADERS += utils.h qmlutils.h elfreader.h

CONFIG += force_bootstrap

win32: LIBS += -lshlwapi -luser32 -lOle32

CONFIG(debug, debug | release) : TARGET = '$$TARGET'd
#message($$TARGET)
