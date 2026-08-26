VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += dll
DEFINES += VISIONAIFLOW_TRAINING_STATE_LIBRARY
QT += core
TARGET = TrainingState
DESTDIR = $$BUILDLIB
OBJECTS_DIR = $$PWD/release
MOC_DIR = $$PWD/release
RCC_DIR = $$PWD/release
UI_DIR = $$PWD/release
QMAKE_POST_LINK += copy /Y $$BUILDLIB_NATIVE\\TrainingState.dll $$VAF_ROOT_NATIVE\\BINX64\\TrainingState.dll
QMAKE_CXXFLAGS += /external:W0 /external:I$$VAF_TORCH_ROOT/include /wd4100 /wd4127 /wd4244 /wd4251 /wd4267 /wd4275 /wd4702
SOURCES += src/TrainingCheckpointState.cpp
HEADERS += include/visionaiflow/training/TrainingCheckpointState.h
INCLUDEPATH += $$VAF_TORCH_ROOT/include \
               $$VAF_TORCH_ROOT/include/torch/csrc/api/include \
               $$PWD/include
PRE_TARGETDEPS += $$BUILDLIB/Foundation.lib
LIBS += -lFoundation -L$$VAF_TORCH_ROOT/lib -ltorch_cpu -lc10
