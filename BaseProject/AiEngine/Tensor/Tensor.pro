VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += dll
DEFINES += VISIONAIFLOW_TENSOR_LIBRARY
QT += core
TARGET = Tensor
DESTDIR = $$BUILDLIB
OBJECTS_DIR = $$PWD/release
MOC_DIR = $$PWD/release
RCC_DIR = $$PWD/release
UI_DIR = $$PWD/release
QMAKE_POST_LINK += copy /Y $$BUILDLIB_NATIVE\\Tensor.dll $$VAF_ROOT_NATIVE\\BINX64\\Tensor.dll
SOURCES += src/GpuLease.cpp
HEADERS += include/visionaiflow/tensor/GpuLease.h
PRE_TARGETDEPS += $$BUILDLIB/Foundation.lib
LIBS += -lFoundation
