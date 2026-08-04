VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += staticlib
QT += core
TARGET = vaf_training
DESTDIR = $$VAF_LIBRARY_DIR
QMAKE_CXXFLAGS += /external:W0 /external:I$$VAF_TORCH_ROOT/include /wd4100 /wd4127 /wd4244 /wd4251 /wd4267 /wd4275 /wd4702
SOURCES += src/LinearClassifierTrainer.cpp \
           src/TensorDataLoader.cpp \
           src/AmpController.cpp \
           src/TrainingCheckpointState.cpp \
           src/AsyncClassificationJob.cpp \
           src/Yolo11DetectionTraining.cpp
HEADERS += include/visionaiflow/training/LinearClassifierTrainer.h \
           include/visionaiflow/training/TensorDataLoader.h \
           include/visionaiflow/training/AmpController.h \
           include/visionaiflow/training/TrainingCheckpointState.h \
           include/visionaiflow/training/AsyncClassificationJob.h \
           include/visionaiflow/training/Yolo11DetectionTraining.h
INCLUDEPATH += $$VAF_TORCH_ROOT/include \
               $$VAF_TORCH_ROOT/include/torch/csrc/api/include
PRE_TARGETDEPS += $$VAF_LIBRARY_DIR/vaf_foundation.lib \
                  $$VAF_LIBRARY_DIR/vaf_models_common.lib
LIBS += -lvaf_foundation -lvaf_models_common -L$$VAF_TORCH_ROOT/lib -ltorch_cpu -lc10
