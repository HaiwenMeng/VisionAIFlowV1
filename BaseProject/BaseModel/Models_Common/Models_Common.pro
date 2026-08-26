VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += dll
DEFINES += VISIONAIFLOW_MODELS_COMMON_LIBRARY
QT += core
TARGET = Models_Common
DESTDIR = $$BUILDLIB
OBJECTS_DIR = $$PWD/release
MOC_DIR = $$PWD/release
RCC_DIR = $$PWD/release
UI_DIR = $$PWD/release
QMAKE_POST_LINK += copy /Y $$BUILDLIB_NATIVE\\Models_Common.dll $$VAF_ROOT_NATIVE\\BINX64\\Models_Common.dll
SOURCES += Classify/src/ClassificationModelAdapter.cpp \
           Detection/src/DetectionPostProcessor.cpp \
           Detection/src/DetectionContracts.cpp
HEADERS += Classify/include/visionaiflow/models/classification/IClassificationModelAdapter.h \
           Detection/include/visionaiflow/models/common/DetectionPostProcessor.h \
           Detection/include/visionaiflow/models/detection/DetectionContracts.h \
           Detection/include/visionaiflow/models/detection/IDetectionModelAdapter.h
PRE_TARGETDEPS += $$BUILDLIB/Foundation.lib \
                  $$BUILDLIB/Models_Api.lib
LIBS += -lFoundation -lModels_Api
