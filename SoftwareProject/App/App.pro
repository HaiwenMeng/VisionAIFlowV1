VAF_ROOT = $$clean_path($$PWD/../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = app
QT += core widgets
TARGET = App
DESTDIR = $$VAF_BINARY_DIR
OBJECTS_DIR = $$PWD/release
MOC_DIR = $$PWD/release
RCC_DIR = $$PWD/release
UI_DIR = $$PWD/release
INCLUDEPATH += $$PWD/include \
               $$VAF_TORCH_ROOT/include \
               $$VAF_TORCH_ROOT/include/torch/csrc/api/include \
               $$VAF_CUDA_QMAKE_ROOT/include \
               $$VAF_OPENVINO_ROOT/include
QMAKE_CXXFLAGS += /utf-8 /wd4996 /external:W0 /external:I$$VAF_TORCH_ROOT/include /external:I$$VAF_OPENVINO_ROOT/include
QMAKE_LFLAGS += /DELAYLOAD:Yolo11.dll /DELAYLOAD:Export.dll
SOURCES += main.cpp \
           src/WorkspaceWindow.cpp \
           src/CreateProjectDialog.cpp \
           src/TrainingController.cpp \
           src/InferenceController.cpp \
           src/AnnotationCanvas.cpp \
           src/ProjectSession.cpp
HEADERS += include/visionaiflow/app/WorkspaceWindow.h \
           include/visionaiflow/app/CreateProjectDialog.h \
           include/visionaiflow/app/TrainingController.h \
           include/visionaiflow/app/InferenceController.h \
           include/visionaiflow/app/AnnotationCanvas.h \
           include/visionaiflow/app/ProjectSession.h
FORMS += forms/CreateProjectDialog.ui \
         forms/InferencePage.ui
PRE_TARGETDEPS += $$BUILDLIB/Foundation.lib \
                  $$BUILDLIB/Domain.lib \
                  $$BUILDLIB/ProjectStore.lib \
                  $$BUILDLIB/Annotation.lib \
                  $$BUILDLIB/Models_Common.lib \
                  $$BUILDLIB/TrainingState.lib \
                  $$BUILDLIB/Training.lib \
                  $$BUILDLIB/Export.lib \
                  $$VAF_MODELS_DIR/Yolo11.lib
LIBS += -lFoundation -lDomain -lProjectStore -lAnnotation -lModels_Common -lTrainingState -lTraining -lExport \
        -L$$VAF_MODELS_DIR -lYolo11 \
        -L$$VAF_TORCH_ROOT/lib -ltorch -ltorch_cpu -ltorch_cuda -lc10 -lc10_cuda \
        -L$$VAF_OPENVINO_ROOT/lib -lopenvino -ldelayimp
VAF_OPENVINO_ROOT_NATIVE = $$replace(VAF_OPENVINO_ROOT, /, \\\\)
VAF_BINARY_DIR_NATIVE = $$replace(VAF_BINARY_DIR, /, \\\\)
QMAKE_POST_LINK += copy /Y $$VAF_OPENVINO_ROOT_NATIVE\\bin\\openvino.dll $$VAF_BINARY_DIR_NATIVE\\openvino.dll \
                   && copy /Y $$VAF_OPENVINO_ROOT_NATIVE\\bin\\openvino_onnx_frontend.dll $$VAF_BINARY_DIR_NATIVE\\openvino_onnx_frontend.dll \
                   && copy /Y $$VAF_OPENVINO_ROOT_NATIVE\\bin\\openvino_intel_cpu_plugin.dll $$VAF_BINARY_DIR_NATIVE\\openvino_intel_cpu_plugin.dll \
                   && copy /Y $$VAF_OPENVINO_ROOT_NATIVE\\bin\\tbb12.dll $$VAF_BINARY_DIR_NATIVE\\tbb12.dll
