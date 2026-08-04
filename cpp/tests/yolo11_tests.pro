VAF_ROOT = $$clean_path($$PWD/../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = app
QT += core testlib
TARGET = vaf_yolo11_tests
DESTDIR = $$VAF_BINARY_DIR
DEFINES += ONNX_NAMESPACE=onnx ONNX_ML
QMAKE_CXXFLAGS += /external:W0 /external:IF:/VisionAIFlowDeps/install/$$VAF_CONFIGURATION/include /external:I$$VAF_TORCH_ROOT/include /wd4100 /wd4127 /wd4244 /wd4251 /wd4267 /wd4275 /wd4702
SOURCES += unit/tst_Yolo11DetectionDecoder.cpp
INCLUDEPATH += F:/VisionAIFlowDeps/install/$$VAF_CONFIGURATION/include \
               $$VAF_TORCH_ROOT/include \
               $$VAF_TORCH_ROOT/include/torch/csrc/api/include
PRE_TARGETDEPS += $$VAF_LIBRARY_DIR/vaf_foundation.lib \
                  $$VAF_LIBRARY_DIR/vaf_models_common.lib \
                  $$VAF_LIBRARY_DIR/vaf_yolo11.lib \
                  $$VAF_LIBRARY_DIR/vaf_training.lib \
                  $$VAF_LIBRARY_DIR/vaf_export.lib
LIBS += -lvaf_foundation -lvaf_models_common -lvaf_yolo11 -lvaf_training -lvaf_export \
        -LF:/VisionAIFlowDeps/install/$$VAF_CONFIGURATION/lib -lonnx -lonnx_proto -llibprotobuf \
        -L$$VAF_TORCH_ROOT/lib -ltorch_cpu -lc10
