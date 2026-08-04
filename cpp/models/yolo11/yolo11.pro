VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += staticlib
TARGET = vaf_yolo11
DESTDIR = $$VAF_LIBRARY_DIR
SOURCES += src/Yolo11DetectionDecoder.cpp
HEADERS += include/visionaiflow/models/yolo11/Yolo11DetectionDecoder.h
PRE_TARGETDEPS += $$VAF_LIBRARY_DIR/vaf_foundation.lib \
                  $$VAF_LIBRARY_DIR/vaf_models_common.lib
LIBS += -lvaf_foundation -lvaf_models_common
