VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += dll
DEFINES += VISIONAIFLOW_YOLO11_LIBRARY
QT += core
TARGET = Yolo11
DESTDIR = $$VAF_MODELS_DIR
OBJECTS_DIR = $$PWD/release
MOC_DIR = $$PWD/release
RCC_DIR = $$PWD/release
UI_DIR = $$PWD/release
QMAKE_CXXFLAGS += /external:I$$VAF_TORCH_ROOT/include
SOURCES += src/Yolo11DetectionDecoder.cpp \
           src/Yolo11DetectionAdapter.cpp \
           src/Yolo11Detector.cpp
HEADERS += include/visionaiflow/models/yolo11/Yolo11DetectionDecoder.h \
           include/visionaiflow/models/yolo11/Yolo11DetectionAdapter.h \
           include/visionaiflow/models/yolo11/Yolo11Detector.h
INCLUDEPATH += $$VAF_TORCH_ROOT/include \
               $$VAF_TORCH_ROOT/include/torch/csrc/api/include \
               $$PWD/include
PRE_TARGETDEPS += $$BUILDLIB/Foundation.lib \
                  $$BUILDLIB/Models_Common.lib \
                  $$BUILDLIB/Models_Api.lib \
                  $$BUILDLIB/TrainingState.lib
LIBS += -lFoundation -lModels_Common -lModels_Api -lTrainingState -L$$VAF_TORCH_ROOT/lib -ltorch_cpu -lc10
