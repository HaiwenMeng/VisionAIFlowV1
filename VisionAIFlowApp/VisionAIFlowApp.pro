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

VAF_APP_MODELS_DIR = $$VAF_ROOT/BINX64/model
VAF_DEPENDS_DIR = $$VAF_ROOT/BaseLibX64/V4depends
VAF_OPENCV_DIR = $$VAF_DEPENDS_DIR/Opencv
VAF_SAM3_DIR = $$VAF_ROOT/LVMs/TrtSAM3Lib

INCLUDEPATH += $$PWD \
               $$PWD/TrainForms/DetecForms \
                 $$VAF_ROOT/UIProject/BaseAnnoDisp \
                 $$PWD/AnnoForms/SemiAutoAnnoForm \
                 $$VAF_SAM3_DIR \
                 $$VAF_SAM3_DIR/third_party/sam3src \
                 $$VAF_DEPENDS_DIR/YtRoiShowDisp \
               $$VAF_DEPENDS_DIR/YtRoiShowDisp/ROIItem \
               $$VAF_DEPENDS_DIR/YtVisionDefine \
               $$VAF_OPENCV_DIR/include \
               $$VAF_TORCH_ROOT/include \
               $$VAF_TORCH_ROOT/include/torch/csrc/api/include \
               $$VAF_CUDA_QMAKE_ROOT/include \
               $$VAF_TENSORRT_ROOT/include \
               $$VAF_OPENVINO_ROOT/include

QMAKE_CXXFLAGS += /utf-8 /wd4189 /wd4457 /wd4828 /wd4996 /wd4456 /wd4458 /wd5054 /external:W0 /external:I$$VAF_TORCH_ROOT/include /external:I$$VAF_OPENVINO_ROOT/include

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
    systemform.cpp \
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
    $$VAF_SAM3_DIR/trtsam3lib.cpp \
    $$VAF_SAM3_DIR/third_party/sam3src/common/createObject.cpp \
    $$VAF_SAM3_DIR/third_party/sam3src/common/image.cpp \
    $$VAF_SAM3_DIR/third_party/sam3src/common/norm.cpp \
    $$VAF_SAM3_DIR/third_party/sam3src/common/object.cpp \
    $$VAF_SAM3_DIR/third_party/sam3src/common/tensorrt.cpp \
    $$VAF_SAM3_DIR/third_party/sam3src/infer/infer.cpp \
    $$VAF_SAM3_DIR/third_party/sam3src/infer/sam3infer.cpp \
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
    systemform.h \
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
    $$VAF_SAM3_DIR/TrtSam3Lib_global.h \
    $$VAF_SAM3_DIR/trtsam3lib.h \
    $$VAF_SAM3_DIR/third_party/sam3src/common/affine.hpp \
    $$VAF_SAM3_DIR/third_party/sam3src/common/check.hpp \
    $$VAF_SAM3_DIR/third_party/sam3src/common/cpm.hpp \
    $$VAF_SAM3_DIR/third_party/sam3src/common/createObject.hpp \
    $$VAF_SAM3_DIR/third_party/sam3src/common/device.hpp \
    $$VAF_SAM3_DIR/third_party/sam3src/common/image.hpp \
    $$VAF_SAM3_DIR/third_party/sam3src/common/memory.hpp \
    $$VAF_SAM3_DIR/third_party/sam3src/common/norm.hpp \
    $$VAF_SAM3_DIR/third_party/sam3src/common/object.hpp \
    $$VAF_SAM3_DIR/third_party/sam3src/common/tensorrt.hpp \
    $$VAF_SAM3_DIR/third_party/sam3src/common/timer.hpp \
    $$VAF_SAM3_DIR/third_party/sam3src/infer/infer.hpp \
    $$VAF_SAM3_DIR/third_party/sam3src/infer/sam3infer.hpp \
    $$VAF_SAM3_DIR/third_party/sam3src/infer/sam3type.hpp \
    $$VAF_SAM3_DIR/third_party/sam3src/kernels/postprocess.cuh \
    $$VAF_SAM3_DIR/third_party/sam3src/kernels/preprocess.cuh \
    $$VAF_SAM3_DIR/third_party/sam3src/kernels/process_kernel_warp.hpp \
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
    systemform.ui \
    setlabelname.ui \
    renamedlg.ui \
    nameselectdlg.ui \
    AnnoForms/SemiAutoAnnoForm/app/MainWindow.ui \
    TrainForms/DetecForms/traindataform.ui \
    AnnoForms/BaseAnnoForm/CommonSetForm.ui

PRE_TARGETDEPS += $$BUILDLIB/Foundation.lib \
                  $$BUILDLIB/Domain.lib \
                  $$BUILDLIB/PluginApi.lib \
                  $$BUILDLIB/BaseAnnoDisp.lib \
                  $$VAF_DEPENDS_DIR/YtRoiShowDisp/release/YtRoiShowDispQt6.lib

LIBS += -lFoundation -lDomain -lPluginApi -lBaseAnnoDisp \
        -ldwmapi -luser32 \
        -L$$VAF_DEPENDS_DIR/YtRoiShowDisp/release -lYtRoiShowDispQt6 \
        -L$$VAF_OPENCV_DIR/lib -lopencv_world460 \
        -L$$VAF_TENSORRT_ROOT/lib -lnvinfer_10 -lnvinfer_plugin_10 -lnvonnxparser_10 \
        -L$$VAF_CUDA_QMAKE_ROOT/lib/x64 -lcudart -lcublas \
        -L$$VAF_TORCH_ROOT/lib -ltorch -ltorch_cpu -ltorch_cuda -lc10 -lc10_cuda \
        -L$$VAF_OPENVINO_ROOT/lib -lopenvino -ldelayimp

CUDA_SOURCES += \
    $$VAF_SAM3_DIR/third_party/sam3src/common/memory.cu \
    $$VAF_SAM3_DIR/third_party/sam3src/kernels/postprocess.cu \
    $$VAF_SAM3_DIR/third_party/sam3src/kernels/preprocess.cu \
    $$VAF_SAM3_DIR/third_party/sam3src/kernels/process_kernel_warp.cu

cuda_compiler.name = CUDA ${QMAKE_FILE_IN}
cuda_compiler.input = CUDA_SOURCES
cuda_compiler.output = $$OBJECTS_DIR/${QMAKE_FILE_BASE}_cuda.obj
cuda_compiler.variable_out = OBJECTS
cuda_compiler.CONFIG += no_link target_predeps
cuda_compiler.dependency_type = TYPE_C
cuda_compiler.commands = $$shell_quote($$VAF_CUDA_ROOT/bin/nvcc.exe) --use-local-env -c -std=c++17 -Xcompiler /utf-8 -Xcompiler /EHsc -Xcompiler /MD -I$$shell_quote($$VAF_CUDA_ROOT/include) -I$$shell_quote($$VAF_TENSORRT_ROOT/include) -I$$shell_quote($$VAF_OPENCV_DIR/include) -I$$shell_quote($$VAF_SAM3_DIR/third_party/sam3src) -o $$shell_quote(${QMAKE_FILE_OUT}) $$shell_quote(${QMAKE_FILE_IN})
QMAKE_EXTRA_COMPILERS += cuda_compiler

VAF_OPENVINO_ROOT_NATIVE = $$replace(VAF_OPENVINO_ROOT, /, \\\\)
VAF_BINARY_DIR_NATIVE = $$replace(VAF_BINARY_DIR, /, \\\\)
VAF_OPENCV_DIR_NATIVE = F:\\VisionAIFlowV1\\BaseLibX64\\V4depends\\Opencv
VAF_APP_MODELS_DIR_NATIVE = F:\\VisionAIFlowV1\\BINX64\\model
QMAKE_POST_LINK += copy /Y $$VAF_OPENVINO_ROOT_NATIVE\\bin\\openvino.dll $$VAF_BINARY_DIR_NATIVE\\openvino.dll \
                   && copy /Y $$VAF_OPENVINO_ROOT_NATIVE\\bin\\openvino_onnx_frontend.dll $$VAF_BINARY_DIR_NATIVE\\openvino_onnx_frontend.dll \
                   && copy /Y $$VAF_OPENVINO_ROOT_NATIVE\\bin\\openvino_intel_cpu_plugin.dll $$VAF_BINARY_DIR_NATIVE\\openvino_intel_cpu_plugin.dll \
                   && copy /Y $$VAF_OPENVINO_ROOT_NATIVE\\bin\\tbb12.dll $$VAF_BINARY_DIR_NATIVE\\tbb12.dll \
                   && copy /Y $$VAF_OPENCV_DIR_NATIVE\\bin\\opencv_world460.dll $$VAF_BINARY_DIR_NATIVE\\opencv_world460.dll
QMAKE_POST_LINK += && copy /Y F:\\VisionAIFlowV1\\BuildLib\\BaseAnnoDisp.dll $$VAF_BINARY_DIR_NATIVE\\BaseAnnoDisp.dll
QMAKE_POST_LINK += && copy /Y F:\\VisionAIFlowV1\\VisionAIFlowApp\\ResourYolo\\Arial.ttf $$VAF_BINARY_DIR_NATIVE\\Arial.ttf
QMAKE_POST_LINK += && copy /Y F:\\VisionAIFlowV1\\VisionAIFlowApp\\ResourYolo\\Arial.Unicode.ttf $$VAF_BINARY_DIR_NATIVE\\Arial.Unicode.ttf

RESOURCES += \
    YtSRes.qrc \
    AnnoForms/SemiAutoAnnoForm/resources.qrc
