VAF_ROOT = $$clean_path($$PWD/../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG -= c++20
CONFIG += c++17 dll

QT += core gui

TARGET = TrtSAM2Lib

DEFINES += TRTSAM2LIB_LIBRARY NOMINMAX

QMAKE_CXXFLAGS += /utf-8 /wd4189 /wd4505 /wd4715

DESTDIR = $$BUILDLIB
OBJECTS_DIR = $$PWD/release
MOC_DIR = $$PWD/release
RCC_DIR = $$PWD/release
UI_DIR = $$PWD/release

VAF_OPENCV_DIR = $$VAF_ROOT/BaseLibX64/V4depends/Opencv

SOURCES += \
    trtsam2lib.cpp \
    third_party/sam2src/infer.cpp \
    third_party/sam2src/sam2.cpp \
    third_party/sam2src/sam2_encoder.cpp \
    third_party/sam2src/sam2_decoder.cpp \
    third_party/sam2src/utils.cpp

HEADERS += \
    TrtSam2Lib_global.h \
    trtsam2lib.h \
    third_party/sam2src/automatic_mask_generator.h \
    third_party/sam2src/infer.h \
    third_party/sam2src/sam2.h \
    third_party/sam2src/sam2_encoder.h \
    third_party/sam2src/sam2_decoder.h \
    third_party/sam2src/utils.h

INCLUDEPATH += \
    $$PWD \
    $$PWD/third_party/sam2src \
    $$VAF_OPENCV_DIR/include \
    $$VAF_TENSORRT_ROOT/include \
    $$VAF_CUDA_QMAKE_ROOT/include

LIBS += \
    -L$$VAF_OPENCV_DIR/lib -lopencv_world460 \
    -L$$VAF_TENSORRT_ROOT/lib -lnvinfer_10 -lnvinfer_plugin_10 -lnvonnxparser_10 \
    -L$$VAF_CUDA_QMAKE_ROOT/lib/x64 -lcudart -lcublas

QMAKE_POST_LINK += copy /Y $$BUILDLIB_NATIVE\\TrtSAM2Lib.dll $$VAF_ROOT_NATIVE\\BINX64\\TrtSAM2Lib.dll
