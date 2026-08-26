VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += dll
DEFINES += VISIONAIFLOW_EXPORT_LIBRARY
QT += core
TARGET = Export
DESTDIR = $$BUILDLIB
OBJECTS_DIR = $$PWD/release
MOC_DIR = $$PWD/release
RCC_DIR = $$PWD/release
UI_DIR = $$PWD/release
QMAKE_POST_LINK += copy /Y $$BUILDLIB_NATIVE\\Export.dll $$VAF_ROOT_NATIVE\\BINX64\\Export.dll
DEFINES += ONNX_NAMESPACE=onnx ONNX_ML
QMAKE_CXXFLAGS += /external:I$$VAF_DEPS_RELEASE_ROOT/include /external:I$$VAF_TORCH_ROOT/include
SOURCES += src/OnnxExporter.cpp \
           src/ModelPackage.cpp
HEADERS += include/visionaiflow/export/OnnxExporter.h \
           include/visionaiflow/export/ModelPackage.h
INCLUDEPATH += $$VAF_DEPS_RELEASE_ROOT/include \
               $$VAF_TORCH_ROOT/include \
               $$VAF_TORCH_ROOT/include/torch/csrc/api/include \
               $$VAF_ROOT/ModelPlugins/Detection/Yolo11/include
PRE_TARGETDEPS += $$BUILDLIB/Foundation.lib \
                  $$BUILDLIB/Models_Common.lib \
                  $$BUILDLIB/TrainingState.lib \
                  $$VAF_MODELS_DIR/Yolo11.lib
LIBS += -lFoundation -lModels_Common -lTrainingState \
        -L$$VAF_MODELS_DIR -lYolo11 \
        -L$$VAF_DEPS_RELEASE_ROOT/lib -lonnx -lonnx_proto -llibprotobuf \
        -L$$VAF_TORCH_ROOT/lib -ltorch_cpu -lc10
