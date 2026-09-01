VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)
include($$VAF_ROOT/qmake/libtorch_cuda.pri)

TEMPLATE = app
QT += core testlib
TARGET = Amp_Tests
OBJECTS_DIR = $$PWD/release
DESTDIR = $$VAF_BINARY_DIR
QMAKE_CXXFLAGS += /external:W0 /external:I$$VAF_TORCH_ROOT/include /wd4100 /wd4127 /wd4244 /wd4251 /wd4267 /wd4275 /wd4702
SOURCES += tst_AmpController.cpp
INCLUDEPATH += $$VAF_TORCH_ROOT/include \
               $$VAF_TORCH_ROOT/include/torch/csrc/api/include
PRE_TARGETDEPS += $$BUILDLIB/VisionAIFlowCore.lib \
                  $$BUILDLIB/Models_Common.lib \
                  $$VAF_MODELS_DIR/Linear.lib \
                  $$BUILDLIB/Training.lib
LIBS += -lVisionAIFlowCore -lModels_Common -L$$VAF_MODELS_DIR -lLinear -lTraining
