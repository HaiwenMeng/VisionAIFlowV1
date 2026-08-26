VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += dll
DEFINES += VISIONAIFLOW_FOUNDATION_LIBRARY
TARGET = Foundation
DESTDIR = $$BUILDLIB
OBJECTS_DIR = $$PWD/release
MOC_DIR = $$PWD/release
RCC_DIR = $$PWD/release
UI_DIR = $$PWD/release
QMAKE_POST_LINK += copy /Y $$BUILDLIB_NATIVE\\Foundation.dll $$VAF_ROOT_NATIVE\\BINX64\\Foundation.dll
SOURCES += src/Error.cpp
HEADERS += include/visionaiflow/foundation/Error.h \
           include/visionaiflow/foundation/Result.h
