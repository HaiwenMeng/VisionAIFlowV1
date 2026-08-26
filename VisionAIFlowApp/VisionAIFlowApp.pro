VAF_ROOT = $$clean_path($$PWD/..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = app
TARGET = VisionAIFlowApp
QT += core gui widgets charts concurrent uitools core5compat
CONFIG += c++17 release
CONFIG -= debug_and_release debug
DEFINES += QDESIGNER_EXPORT_WIDGETS TRTSAM3LIB_LIBRARY

DESTDIR = $$VAF_BINARY_DIR
OBJECTS_DIR = $$PWD/release
MOC_DIR = $$PWD/release
RCC_DIR = $$PWD/release
UI_DIR = $$PWD/release

VAF_APP_MODELS_DIR = $$VAF_ROOT/BINX64/Models
VAF_DEPENDS_DIR = $$VAF_ROOT/BaseLibX64/V4depends
VAF_OPENCV_DIR = $$VAF_DEPENDS_DIR/Opencv

INCLUDEPATH += $$PWD \
               $$PWD/TrainForms/DetecForms \
                 $$VAF_ROOT/UIProject/BaseAnnoDisp \
                 $$VAF_DEPENDS_DIR/Sam3/TrtSam3Lib \
                 $$VAF_DEPENDS_DIR/Sam3/SamBaseLib \
                 $$PWD/AnnoForms/SemiAutoAnnoForm \
                 $$VAF_DEPENDS_DIR/YtRoiShowDisp \
               $$VAF_DEPENDS_DIR/YtRoiShowDisp/ROIItem \
               $$VAF_DEPENDS_DIR/YtVisionDefine \
               $$VAF_OPENCV_DIR/include \
               $$VAF_ROOT/SoftwareProject/App/include \
               $$VAF_TORCH_ROOT/include \
               $$VAF_TORCH_ROOT/include/torch/csrc/api/include \
               $$VAF_CUDA_QMAKE_ROOT/include \
               $$VAF_OPENVINO_ROOT/include

QMAKE_CXXFLAGS += /utf-8 /wd4189 /wd4828 /wd4996 /wd4456 /wd4458 /wd5054 /external:W0 /external:I$$VAF_TORCH_ROOT/include /external:I$$VAF_OPENVINO_ROOT/include
QMAKE_LFLAGS += /DELAYLOAD:Yolo11.dll /DELAYLOAD:Export.dll

SOURCES += \
    main.cpp \
    maindlg.cpp \
    projectform.cpp \
    setnamedialog.cpp \
    datasetform.cpp \
    valsetform.cpp \
    taskrepository.cpp \
    ytyolodefine.cpp \
    mlsdprocesser.cpp \
    showprocessform.cpp \
    filecopysetdlg.cpp \
    ytchartview.cpp \
    linechart.cpp \
    setlabelname.cpp \
    renamedlg.cpp \
    nameselectdlg.cpp \
    baseannolegacycanvas.cpp \
    AnnoForms/SemiAutoAnnoForm/baseimageannotatewidget.cpp \
    AnnoForms/SemiAutoAnnoForm/app/MainWindow.cpp \
    AnnoForms/SemiAutoAnnoForm/app/StartupOverlay.cpp \
    AnnoForms/SemiAutoAnnoForm/app/UiTheme.cpp \
    AnnoForms/SemiAutoAnnoForm/data/AnnotationJsonIO.cpp \
    AnnoForms/SemiAutoAnnoForm/data/LabelConfigIO.cpp \
    AnnoForms/SemiAutoAnnoForm/dialogs/AddLabelDialog.cpp \
    AnnoForms/SemiAutoAnnoForm/dialogs/LabelSelectDialog.cpp \
    AnnoForms/SemiAutoAnnoForm/inference/SamInferenceBridge.cpp \
    AnnoForms/SemiAutoAnnoForm/inference/SamInferenceWorker.cpp \
    TrainForms/DetecForms/traindataform.cpp \
    TrainForms/DetecForms/detecttrainingcontroller.cpp \
    AnnoForms/BaseAnnoForm/CommonSetForm.cpp

HEADERS += \
    maindlg.h \
    projectform.h \
    setnamedialog.h \
    datasetform.h \
    valsetform.h \
    taskrepository.h \
    ytyolodefine.h \
    mlsdprocesser.h \
    showprocessform.h \
    filecopysetdlg.h \
    ytchartview.h \
    linechart.h \
    setlabelname.h \
    renamedlg.h \
    nameselectdlg.h \
    baseannolegacycanvas.h \
    AnnoForms/SemiAutoAnnoForm/baseimageannotatewidget.h \
    AnnoForms/SemiAutoAnnoForm/app/AppTypes.h \
    AnnoForms/SemiAutoAnnoForm/app/MainWindow.h \
    AnnoForms/SemiAutoAnnoForm/app/StartupOverlay.h \
    AnnoForms/SemiAutoAnnoForm/app/UiTheme.h \
    AnnoForms/SemiAutoAnnoForm/data/AnnotationJsonIO.h \
    AnnoForms/SemiAutoAnnoForm/data/LabelConfigIO.h \
    AnnoForms/SemiAutoAnnoForm/dialogs/AddLabelDialog.h \
    AnnoForms/SemiAutoAnnoForm/dialogs/LabelSelectDialog.h \
    AnnoForms/SemiAutoAnnoForm/inference/SamInferenceBridge.h \
    AnnoForms/SemiAutoAnnoForm/inference/SamInferenceWorker.h \
    AnnoForms/SemiAutoAnnoForm/inference/SamTypes.h \
    TrainForms/DetecForms/traindataform.h \
    TrainForms/DetecForms/detecttrainingcontroller.h \
    AnnoForms/BaseAnnoForm/CommonSetForm.h

FORMS += \
    maindlg.ui \
    projectform.ui \
    setnamedialog.ui \
    datasetform.ui \
    valsetform.ui \
    showprocessform.ui \
    filecopysetdlg.ui \
    setlabelname.ui \
    renamedlg.ui \
    nameselectdlg.ui \
    AnnoForms/SemiAutoAnnoForm/app/MainWindow.ui \
    TrainForms/DetecForms/traindataform.ui \
    AnnoForms/BaseAnnoForm/CommonSetForm.ui

PRE_TARGETDEPS += $$BUILDLIB/Foundation.lib \
                  $$BUILDLIB/Domain.lib \
                  $$BUILDLIB/Models_Common.lib \
                  $$BUILDLIB/TrainingState.lib \
                  $$BUILDLIB/Training.lib \
                  $$BUILDLIB/Export.lib \
                  $$VAF_APP_MODELS_DIR/Yolo11.lib \
                  $$BUILDLIB/BaseAnnoDisp.lib \
                  $$VAF_DEPENDS_DIR/Sam3/lib/SamBaseLib.lib \
                  $$VAF_DEPENDS_DIR/Sam3/lib/TrtSam3Lib.lib \
                  $$VAF_DEPENDS_DIR/YtRoiShowDisp/release/YtRoiShowDispQt6.lib

LIBS += -lFoundation -lDomain -lModels_Common -lTrainingState -lTraining -lExport -lBaseAnnoDisp \
        -L$$VAF_DEPENDS_DIR/Sam3/lib -lSamBaseLib -lTrtSam3Lib \
        -ldwmapi -luser32 \
        -L$$VAF_APP_MODELS_DIR -lYolo11 \
        -L$$VAF_DEPENDS_DIR/YtRoiShowDisp/release -lYtRoiShowDispQt6 \
        -L$$VAF_OPENCV_DIR/lib -lopencv_world460 \
        -L$$VAF_TORCH_ROOT/lib -ltorch -ltorch_cpu -ltorch_cuda -lc10 -lc10_cuda \
        -L$$VAF_OPENVINO_ROOT/lib -lopenvino -ldelayimp

VAF_OPENVINO_ROOT_NATIVE = $$replace(VAF_OPENVINO_ROOT, /, \\\\)
VAF_BINARY_DIR_NATIVE = $$replace(VAF_BINARY_DIR, /, \\\\)
VAF_OPENCV_DIR_NATIVE = F:\\VisionAIFlowV1\\BaseLibX64\\V4depends\\Opencv
VAF_APP_MODELS_DIR_NATIVE = F:\\VisionAIFlowV1\\BINX64\\Models
QMAKE_POST_LINK += copy /Y $$VAF_OPENVINO_ROOT_NATIVE\\bin\\openvino.dll $$VAF_BINARY_DIR_NATIVE\\openvino.dll \
                   && copy /Y $$VAF_OPENVINO_ROOT_NATIVE\\bin\\openvino_onnx_frontend.dll $$VAF_BINARY_DIR_NATIVE\\openvino_onnx_frontend.dll \
                   && copy /Y $$VAF_OPENVINO_ROOT_NATIVE\\bin\\openvino_intel_cpu_plugin.dll $$VAF_BINARY_DIR_NATIVE\\openvino_intel_cpu_plugin.dll \
                   && copy /Y $$VAF_OPENVINO_ROOT_NATIVE\\bin\\tbb12.dll $$VAF_BINARY_DIR_NATIVE\\tbb12.dll \
                   && copy /Y $$VAF_OPENCV_DIR_NATIVE\\bin\\opencv_world460.dll $$VAF_BINARY_DIR_NATIVE\\opencv_world460.dll
QMAKE_POST_LINK += && copy /Y $$VAF_APP_MODELS_DIR_NATIVE\\Yolo11.dll $$VAF_BINARY_DIR_NATIVE\\Yolo11.dll
QMAKE_POST_LINK += && copy /Y F:\\VisionAIFlowV1\\BuildLib\\BaseAnnoDisp.dll $$VAF_BINARY_DIR_NATIVE\\BaseAnnoDisp.dll
QMAKE_POST_LINK += && copy /Y F:\\VisionAIFlowV1\\VisionAIFlowApp\\ResourYolo\\Arial.ttf $$VAF_BINARY_DIR_NATIVE\\Arial.ttf
QMAKE_POST_LINK += && copy /Y F:\\VisionAIFlowV1\\VisionAIFlowApp\\ResourYolo\\Arial.Unicode.ttf $$VAF_BINARY_DIR_NATIVE\\Arial.Unicode.ttf

RESOURCES += \
    YtSRes.qrc \
    AnnoForms/SemiAutoAnnoForm/resources.qrc
