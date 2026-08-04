VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += staticlib
TARGET = vaf_models_common
DESTDIR = $$VAF_LIBRARY_DIR
SOURCES += src/DetectionPostProcessor.cpp
HEADERS += include/visionaiflow/models/common/DetectionPostProcessor.h
PRE_TARGETDEPS += $$VAF_LIBRARY_DIR/vaf_foundation.lib
LIBS += -lvaf_foundation
