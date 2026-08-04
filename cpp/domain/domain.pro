VAF_ROOT = $$clean_path($$PWD/../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += staticlib
TARGET = vaf_domain
DESTDIR = $$VAF_LIBRARY_DIR
SOURCES += src/JobState.cpp
HEADERS += include/visionaiflow/domain/JobState.h
PRE_TARGETDEPS += $$VAF_LIBRARY_DIR/vaf_foundation.lib
LIBS += -lvaf_foundation
