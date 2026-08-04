VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += staticlib
QT += core
TARGET = vaf_tensor
DESTDIR = $$VAF_LIBRARY_DIR
SOURCES += src/GpuLease.cpp
HEADERS += include/visionaiflow/tensor/GpuLease.h
PRE_TARGETDEPS += $$VAF_LIBRARY_DIR/vaf_foundation.lib
LIBS += -lvaf_foundation
