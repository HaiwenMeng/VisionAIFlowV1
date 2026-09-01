VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

!exists($$VAF_DEPS_RELEASE_ROOT/include/spdlog/sinks/rotating_file_sink.h): error(spdlog Release headers are missing at $$VAF_DEPS_RELEASE_ROOT)

TEMPLATE = lib
CONFIG += dll
DEFINES += VISIONAIFLOW_QT_FOUNDATION_LIBRARY
QT += core network
TARGET = QtFoundation
DESTDIR = $$BUILDLIB
OBJECTS_DIR = $$PWD/release
MOC_DIR = $$PWD/release
RCC_DIR = $$PWD/release
UI_DIR = $$PWD/release
QMAKE_POST_LINK += copy /Y $$BUILDLIB_NATIVE\\QtFoundation.dll $$VAF_ROOT_NATIVE\\BINX64\\QtFoundation.dll
SOURCES += src/StructuredLogger.cpp \
           src/HostRuntime.cpp
HEADERS += include/visionaiflow/qt_foundation/StructuredLogger.h \
           include/visionaiflow/qt_foundation/HostRuntime.h \
           include/visionaiflow/qt_foundation/HostInferenceContract.h
PRE_TARGETDEPS += $$BUILDLIB/VisionAIFlowCore.lib \
                   $$BUILDLIB/Ipc.lib
INCLUDEPATH += $$VAF_DEPS_RELEASE_ROOT/include
LIBS += -lVisionAIFlowCore -lIpc $$VAF_DEPS_RELEASE_ROOT/lib/spdlog.lib
QMAKE_CXXFLAGS += -utf-8 /wd4459
