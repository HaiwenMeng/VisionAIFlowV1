VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += dll
DEFINES += VISIONAIFLOW_DOMAIN_LIBRARY
TARGET = Domain
DESTDIR = $$BUILDLIB
OBJECTS_DIR = $$PWD/release
MOC_DIR = $$PWD/release
RCC_DIR = $$PWD/release
UI_DIR = $$PWD/release
QMAKE_POST_LINK += copy /Y $$BUILDLIB_NATIVE\\Domain.dll $$VAF_ROOT_NATIVE\\BINX64\\Domain.dll
SOURCES += src/JobState.cpp \
           src/ProjectType.cpp
HEADERS += include/visionaiflow/domain/JobState.h \
           include/visionaiflow/domain/ProjectType.h
PRE_TARGETDEPS += $$BUILDLIB/Foundation.lib
LIBS += -lFoundation
