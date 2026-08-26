CONFIG      += plugin debug_and_release
TARGET      = $$qtLibraryTarget(ytroishowdispplugin)
TEMPLATE    = lib

HEADERS     = ytroishowdispplugin.h
SOURCES     = ytroishowdispplugin.cpp
RESOURCES   = icons.qrc
LIBS        += -L. 

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += designer
} else {
    CONFIG += designer
}

target.path = $$[QT_INSTALL_PLUGINS]/designer
INSTALLS    += target
INCLUDEPATH+=../YtVisionDefine\
             ./ROIItem


include(ytroishowdisp.pri)
include(../../../BaseLib.pri)
DESTDIR=$${DESTPATH}

DEFINES += QT_NO_WARNING_OUTPUT
DEFINES += QT_NO_DEBUG_OUTPUT
