VAF_ROOT = $$clean_path($$PWD/../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += staticlib
QT += core network
TARGET = vaf_qt_foundation
DESTDIR = $$VAF_LIBRARY_DIR
SOURCES += src/StructuredLogger.cpp \
           src/HostRuntime.cpp
HEADERS += include/visionaiflow/qt_foundation/StructuredLogger.h \
           include/visionaiflow/qt_foundation/HostRuntime.h
PRE_TARGETDEPS += $$VAF_LIBRARY_DIR/vaf_foundation.lib \
                  $$VAF_LIBRARY_DIR/vaf_ipc.lib
INCLUDEPATH += $$VAF_DEPS_ROOT/install/$$VAF_CONFIGURATION/include
LIBS += -lvaf_foundation -lvaf_ipc $$VAF_DEPS_ROOT/install/$$VAF_CONFIGURATION/lib/spdlog.lib
QMAKE_CXXFLAGS += -utf-8 /wd4459
