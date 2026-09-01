VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += dll
DEFINES += VISIONAIFLOW_YOLOV8_LIBRARY
QT += core gui
TARGET = YoloV8
DESTDIR = $$VAF_AI_MODEL_PLUGINS_DIR
OBJECTS_DIR = $$PWD/release
MOC_DIR = $$PWD/release
RCC_DIR = $$PWD/release
UI_DIR = $$PWD/release
QMAKE_CXXFLAGS += /external:I$$VAF_TORCH_ROOT/include
QMAKE_CXXFLAGS += /utf-8
SOURCES += third_party/koba_jon/networks.cpp \
           third_party/koba_jon/loss.cpp \
           third_party/koba_jon/losses.cpp \
           yolov8trainer.cpp \
           yolov8trainplugin.cpp
HEADERS += third_party/koba_jon/networks.hpp \
           third_party/koba_jon/loss.hpp \
           third_party/koba_jon/losses.hpp \
           yolov8trainer.h \
           yolov8trainplugin.h
DISTFILES += yolov8trainplugin.json
INCLUDEPATH += $$VAF_TORCH_ROOT/include \
               $$VAF_TORCH_ROOT/include/torch/csrc/api/include \
               $$VAF_CUDA_QMAKE_ROOT/include \
               $$PWD/third_party/koba_jon
PRE_TARGETDEPS += $$BUILDLIB/PluginApi.lib
LIBS += -lPluginApi \
        -L$$VAF_CUDA_QMAKE_ROOT/lib/x64 -lcudart \
        -L$$VAF_TORCH_ROOT/lib -ltorch -ltorch_cpu -ltorch_cuda -lc10 -lc10_cuda
