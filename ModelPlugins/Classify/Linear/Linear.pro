VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += dll
DEFINES += VISIONAIFLOW_LINEAR_LIBRARY
QT += core
TARGET = Linear
DESTDIR = $$VAF_MODELS_DIR
OBJECTS_DIR = $$PWD/release
MOC_DIR = $$PWD/release
RCC_DIR = $$PWD/release
UI_DIR = $$PWD/release
QMAKE_CXXFLAGS += /external:I$$VAF_TORCH_ROOT/include
SOURCES += src/LinearClassifier.cpp \
           src/LinearClassificationAdapter.cpp \
           src/RegisterLinearClassificationAdapter.cpp \
           $$VAF_ROOT/BaseProject/AiEngine/Training/src/AsyncClassificationJob.cpp
HEADERS += include/visionaiflow/models/classification/linear/LinearClassifier.h \
           include/visionaiflow/models/classification/linear/LinearClassificationAdapter.h \
           include/visionaiflow/models/classification/linear/RegisterLinearClassificationAdapter.h \
           $$VAF_ROOT/BaseProject/AiEngine/Training/include/visionaiflow/training/AsyncClassificationJob.h \
           $$VAF_ROOT/BaseProject/AiEngine/TrainingState/include/visionaiflow/training/TrainingCheckpointState.h
INCLUDEPATH += $$VAF_TORCH_ROOT/include \
               $$VAF_TORCH_ROOT/include/torch/csrc/api/include \
               $$PWD/include
PRE_TARGETDEPS += $$BUILDLIB/Foundation.lib \
                  $$BUILDLIB/Models_Api.lib \
                  $$BUILDLIB/Models_Common.lib \
                  $$BUILDLIB/TrainingState.lib
LIBS += -lFoundation -lModels_Api -lModels_Common -lTrainingState -L$$VAF_TORCH_ROOT/lib -ltorch_cpu -lc10
