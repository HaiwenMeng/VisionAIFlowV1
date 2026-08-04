VAF_ROOT = $$clean_path($$PWD/../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = app
QT += core network
TARGET = VisionTensorRtHost
DESTDIR = $$VAF_BINARY_DIR
QMAKE_CXXFLAGS += /external:W0 /external:I$$VAF_TENSORRT_ROOT/include
SOURCES += src/main.cpp \
           src/TensorRtValidator.cpp
HEADERS += include/visionaiflow/tensorrt_host/TensorRtValidator.h
PRE_TARGETDEPS += $$VAF_LIBRARY_DIR/vaf_foundation.lib \
                  $$VAF_LIBRARY_DIR/vaf_ipc.lib \
                  $$VAF_LIBRARY_DIR/vaf_qt_foundation.lib \
                  $$VAF_LIBRARY_DIR/vaf_models_common.lib \
                  $$VAF_LIBRARY_DIR/vaf_yolo11.lib
INCLUDEPATH += $$VAF_TENSORRT_ROOT/include \
               $$VAF_CUDA_QMAKE_ROOT/include \
               $$VAF_ROOT/cpp/tensorrt_host/include
LIBS += -lvaf_foundation -lvaf_ipc -lvaf_qt_foundation -lvaf_models_common -lvaf_yolo11 \
        -L$$VAF_TENSORRT_ROOT/lib -lnvinfer_10 -lnvonnxparser_10 \
        -L$$VAF_CUDA_QMAKE_ROOT/lib/x64 -lcudart
