VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = app
QT += core testlib
TARGET = vaf_yolo11_tests
DESTDIR = $$VAF_BINARY_DIR
DEFINES += ONNX_NAMESPACE=onnx ONNX_ML \
           VAF_TEST_QT_BIN=\\\"$$VAF_QT_ROOT/bin\\\" \
           VAF_TEST_OPENVINO_BIN=\\\"$$VAF_OPENVINO_ROOT/bin\\\" \
           VAF_TEST_CUDA_BIN=\\\"$$VAF_CUDA_QMAKE_ROOT/bin\\\" \
           VAF_TEST_TENSORRT_LIB=\\\"$$VAF_TENSORRT_ROOT/lib\\\" \
           VAF_TEST_MODELS_DIR=\\\"$$VAF_MODELS_DIR\\\" \
           VAF_TEST_TORCH_LIB=\\\"$$VAF_TORCH_ROOT/lib\\\"
QMAKE_CXXFLAGS += /external:W0 /external:I$$VAF_DEPS_RELEASE_ROOT/include /external:I$$VAF_TORCH_ROOT/include /wd4100 /wd4127 /wd4244 /wd4251 /wd4267 /wd4275 /wd4702
SOURCES += tst_Yolo11DetectionDecoder.cpp
INCLUDEPATH += $$VAF_DEPS_RELEASE_ROOT/include \
               $$VAF_TORCH_ROOT/include \
               $$VAF_TORCH_ROOT/include/torch/csrc/api/include
PRE_TARGETDEPS += $$BUILDLIB/Foundation.lib \
                  $$BUILDLIB/Models_Common.lib \
                  $$BUILDLIB/Models_Api.lib \
                  $$VAF_MODELS_DIR/Yolo11.lib \
                  $$BUILDLIB/Training.lib \
                  $$BUILDLIB/Export.lib \
                  $$BUILDLIB/TrainingState.lib \
                  $$VAF_MODELS_DIR/Linear.lib
LIBS += -lFoundation -lModels_Common -lModels_Api -L$$VAF_MODELS_DIR -lYolo11 -lLinear -lTraining -lExport -lTrainingState \
        -L$$VAF_DEPS_RELEASE_ROOT/lib -lonnx -lonnx_proto -llibprotobuf \
        -L$$VAF_TORCH_ROOT/lib -ltorch_cpu -lc10
