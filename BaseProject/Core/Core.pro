VAF_ROOT = $$clean_path($$PWD/../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += dll
QT += core
DEFINES += VISIONAIFLOW_CORE_LIBRARY
TARGET = VisionAIFlowCore
DESTDIR = $$BUILDLIB
OBJECTS_DIR = $$PWD/release
MOC_DIR = $$PWD/release
RCC_DIR = $$PWD/release
UI_DIR = $$PWD/release
QMAKE_POST_LINK += copy /Y $$BUILDLIB_NATIVE\\VisionAIFlowCore.dll $$VAF_ROOT_NATIVE\\BINX64\\VisionAIFlowCore.dll

SOURCES += src/Error.cpp \
           src/JobState.cpp \
           src/ProjectType.cpp

HEADERS += include/visionaiflow/foundation/Error.h \
           include/visionaiflow/foundation/Result.h \
           include/visionaiflow/domain/JobState.h \
           include/visionaiflow/domain/ProjectType.h
