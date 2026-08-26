VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += dll
DEFINES += VISIONAIFLOW_IPC_LIBRARY
QT += core network
TARGET = Ipc
DESTDIR = $$BUILDLIB
OBJECTS_DIR = $$PWD/release
MOC_DIR = $$PWD/release
RCC_DIR = $$PWD/release
UI_DIR = $$PWD/release
QMAKE_POST_LINK += copy /Y $$BUILDLIB_NATIVE\\Ipc.dll $$VAF_ROOT_NATIVE\\BINX64\\Ipc.dll
SOURCES += src/Protocol.cpp \
           src/LocalServer.cpp \
           src/LocalClient.cpp
HEADERS += include/visionaiflow/ipc/Protocol.h \
           include/visionaiflow/ipc/LocalServer.h \
           include/visionaiflow/ipc/LocalClient.h
PRE_TARGETDEPS += $$BUILDLIB/Foundation.lib
LIBS += -lFoundation
