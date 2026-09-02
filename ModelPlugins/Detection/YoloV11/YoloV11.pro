VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)
include($$VAF_ROOT/qmake/libtorch_cuda.pri)

TEMPLATE = lib
CONFIG += dll
DEFINES += VISIONAIFLOW_YOLOV11_LIBRARY
QT += core gui
TARGET = YoloV11
DESTDIR = $$VAF_AI_MODEL_PLUGINS_DIR
OBJECTS_DIR = $$PWD/release
MOC_DIR = $$PWD/release
RCC_DIR = $$PWD/release
UI_DIR = $$PWD/release
QMAKE_CXXFLAGS += /external:I$$VAF_TORCH_ROOT/include
QMAKE_CXXFLAGS += /utf-8

VAF_OPENCV_DIR = $$VAF_ROOT/BaseLibX64/V4depends/Opencv
INCLUDEPATH += $$VAF_OPENCV_DIR/include
LIBS += -L$$VAF_OPENCV_DIR/lib -lopencv_world460

SOURCES += yolov11network.cpp \
           yolov11loss.cpp \
           yolov11trainer.cpp \
           yolov11inference.cpp \
           yolov11plugin.cpp
HEADERS += yolov11network.h \
           yolov11loss.h \
           yolov11trainer.h \
           yolov11inference.h \
           yolov11plugin.h
DISTFILES += yolov11plugin.json \
             tools/convert_ultralytics_checkpoint.py

PRE_TARGETDEPS += $$BUILDLIB/PluginApi.lib
LIBS += -lPluginApi
