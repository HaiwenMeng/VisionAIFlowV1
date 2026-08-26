VAF_ROOT = $$clean_path($$PWD/../../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += staticlib
TARGET = vaf_detection_common
DESTDIR = $$VAF_LIBRARY_DIR
SOURCES += src/DetectionPostProcessor.cpp \
           src/DetectionContracts.cpp
HEADERS += include/visionaiflow/models/common/DetectionPostProcessor.h \
           include/visionaiflow/models/detection/DetectionContracts.h \
           include/visionaiflow/models/detection/IDetectionModelAdapter.h
INCLUDEPATH += $$VAF_ROOT/cpp/models/api/include \
               $$VAF_ROOT/cpp/models/detection/common/include
PRE_TARGETDEPS += $$VAF_LIBRARY_DIR/vaf_foundation.lib
LIBS += -lvaf_foundation
