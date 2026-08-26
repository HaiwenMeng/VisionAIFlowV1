VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = app
QT += core testlib
TARGET = Training_Tests
OBJECTS_DIR = $$PWD/release
DESTDIR = $$VAF_BINARY_DIR
QMAKE_CXXFLAGS += /external:W0 /external:I$$VAF_TORCH_ROOT/include /wd4100 /wd4127 /wd4244 /wd4251 /wd4267 /wd4275 /wd4702
SOURCES += tst_LinearClassifierTrainer.cpp
INCLUDEPATH += $$VAF_TORCH_ROOT/include \
               $$VAF_TORCH_ROOT/include/torch/csrc/api/include
PRE_TARGETDEPS += $$BUILDLIB/Foundation.lib \
                  $$BUILDLIB/Models_Common.lib \
                  $$BUILDLIB/Models_Api.lib \
                  $$BUILDLIB/TrainingState.lib \
                  $$VAF_MODELS_DIR/Linear.lib \
                  $$BUILDLIB/Training.lib
LIBS += -lFoundation -lModels_Common -lModels_Api -lTrainingState -L$$VAF_MODELS_DIR -lLinear -lTraining -L$$VAF_TORCH_ROOT/lib -ltorch_cpu -lc10
