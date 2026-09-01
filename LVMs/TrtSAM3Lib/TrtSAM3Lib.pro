VAF_ROOT = $$clean_path($$PWD/../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG -= c++20
CONFIG += c++17 dll

QT += core gui

TARGET = TrtSAM3Lib

DEFINES += TRTSAM3LIB_LIBRARY NOMINMAX

QMAKE_CXXFLAGS += /utf-8 /wd4189 /wd4457 /wd4996 /wd5054

DESTDIR = $$BUILDLIB
OBJECTS_DIR = $$PWD/release
MOC_DIR = $$PWD/release
RCC_DIR = $$PWD/release
UI_DIR = $$PWD/release

VAF_OPENCV_DIR = $$VAF_ROOT/BaseLibX64/V4depends/Opencv

SOURCES += \
    trtsam3lib.cpp \
    third_party/sam3src/common/createObject.cpp \
    third_party/sam3src/common/image.cpp \
    third_party/sam3src/common/norm.cpp \
    third_party/sam3src/common/object.cpp \
    third_party/sam3src/common/tensorrt.cpp \
    third_party/sam3src/infer/infer.cpp \
    third_party/sam3src/infer/sam3infer.cpp

HEADERS += \
    TrtSam3Lib_global.h \
    trtsam3lib.h \
    third_party/sam3src/common/affine.hpp \
    third_party/sam3src/common/check.hpp \
    third_party/sam3src/common/cpm.hpp \
    third_party/sam3src/common/createObject.hpp \
    third_party/sam3src/common/device.hpp \
    third_party/sam3src/common/image.hpp \
    third_party/sam3src/common/memory.hpp \
    third_party/sam3src/common/norm.hpp \
    third_party/sam3src/common/object.hpp \
    third_party/sam3src/common/tensorrt.hpp \
    third_party/sam3src/common/timer.hpp \
    third_party/sam3src/infer/infer.hpp \
    third_party/sam3src/infer/sam3infer.hpp \
    third_party/sam3src/infer/sam3type.hpp \
    third_party/sam3src/kernels/postprocess.cuh \
    third_party/sam3src/kernels/preprocess.cuh \
    third_party/sam3src/kernels/process_kernel_warp.hpp

CUDA_SOURCES += \
    third_party/sam3src/common/memory.cu \
    third_party/sam3src/kernels/postprocess.cu \
    third_party/sam3src/kernels/preprocess.cu \
    third_party/sam3src/kernels/process_kernel_warp.cu

INCLUDEPATH += \
    $$PWD \
    $$PWD/third_party/sam3src \
    $$VAF_OPENCV_DIR/include \
    $$VAF_TENSORRT_ROOT/include \
    $$VAF_CUDA_QMAKE_ROOT/include

LIBS += \
    -L$$VAF_OPENCV_DIR/lib -lopencv_world460 \
    -L$$VAF_TENSORRT_ROOT/lib -lnvinfer_10 -lnvinfer_plugin_10 -lnvonnxparser_10 \
    -L$$VAF_CUDA_QMAKE_ROOT/lib/x64 -lcudart -lcublas

cuda_compiler.name = CUDA ${QMAKE_FILE_IN}
cuda_compiler.input = CUDA_SOURCES
cuda_compiler.output = $$OBJECTS_DIR/${QMAKE_FILE_BASE}_cuda.obj
cuda_compiler.variable_out = OBJECTS
cuda_compiler.CONFIG += no_link target_predeps
cuda_compiler.dependency_type = TYPE_C
cuda_compiler.commands = $$shell_quote($$VAF_CUDA_ROOT/bin/nvcc.exe) --use-local-env -c -std=c++17 -Xcompiler /utf-8 -Xcompiler /EHsc -Xcompiler /MD -I$$shell_quote($$VAF_CUDA_ROOT/include) -I$$shell_quote($$VAF_TENSORRT_ROOT/include) -I$$shell_quote($$VAF_OPENCV_DIR/include) -I$$shell_quote($$PWD/third_party/sam3src) -o $$shell_quote(${QMAKE_FILE_OUT}) $$shell_quote(${QMAKE_FILE_IN})
QMAKE_EXTRA_COMPILERS += cuda_compiler

QMAKE_POST_LINK += copy /Y $$BUILDLIB_NATIVE\\TrtSAM3Lib.dll $$VAF_ROOT_NATIVE\\BINX64\\TrtSAM3Lib.dll
