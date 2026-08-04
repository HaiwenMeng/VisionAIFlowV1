VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += staticlib
QT += core network
TARGET = vaf_ipc
DESTDIR = $$VAF_LIBRARY_DIR
SOURCES += src/Protocol.cpp \
           src/LocalServer.cpp \
           src/LocalClient.cpp
HEADERS += include/visionaiflow/ipc/Protocol.h \
           include/visionaiflow/ipc/LocalServer.h \
           include/visionaiflow/ipc/LocalClient.h
PRE_TARGETDEPS += $$VAF_LIBRARY_DIR/vaf_foundation.lib
LIBS += -lvaf_foundation
