isEmpty(VAF_ROOT): error(VAF_ROOT must be set by the including .pro file)

VAF_DEPS_ROOT = F:/VisionAIFlowDeps
VAF_QT_ROOT = F:/Qt6.7.3/6.7.3/msvc2019_64
VAF_CUDA_ROOT = C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.8
VAF_CUDA_QMAKE_ROOT = C:/PROGRA~1/NVIDIA~2/CUDA/v11.8
VAF_TENSORRT_ROOT = E:/TensorRT-10.13.0.35
VAF_OPENVINO_ROOT = F:/VisionAIFlowDeps/openvino2025.3.0
VAF_TORCH_ROOT = F:/VisionAIFlowDeps/libtorch/2.7.1-cu118/release
VAF_DEPS_RELEASE_ROOT = $$VAF_DEPS_ROOT/install/Release

CONFIG += release c++20 no_utf8_source
CONFIG -= debug debug_and_release staticlib warn_on
!contains(CONFIG, release): error(Only the Release configuration is supported.)

!exists($$VAF_QT_ROOT/bin/qmake.exe): error(Qt 6.7.3 qmake is missing at $$VAF_QT_ROOT/bin/qmake.exe)
!exists($$VAF_CUDA_ROOT/bin/nvcc.exe): error(CUDA 11.8 nvcc is missing at $$VAF_CUDA_ROOT/bin/nvcc.exe)
!exists($$VAF_TENSORRT_ROOT/include/NvInferVersion.h): error(TensorRT headers are missing at $$VAF_TENSORRT_ROOT)
!exists($$VAF_OPENVINO_ROOT/include/openvino/openvino.hpp): error(OpenVINO headers are missing at $$VAF_OPENVINO_ROOT)
!exists($$VAF_TORCH_ROOT/include/torch/csrc/api/include/torch/version.h): error(LibTorch 2.7.1 cu118 Release headers are missing at $$VAF_TORCH_ROOT)
!exists($$VAF_DEPS_RELEASE_ROOT/include/spdlog/sinks/rotating_file_sink.h): error(spdlog Release headers are missing at $$VAF_DEPS_RELEASE_ROOT)

DESTDIR = $$VAF_ROOT/BINX64
BUILDLIB = $$VAF_ROOT/BuildLib
VAF_MODELS_DIR = $$DESTDIR/model
VAF_LIBRARY_DIR = $$BUILDLIB
VAF_BINARY_DIR = $$DESTDIR
VAF_ROOT_NATIVE = $$replace(VAF_ROOT, /, \\\\)
BUILDLIB_NATIVE = $$replace(BUILDLIB, /, \\\\)

QMAKE_CXXFLAGS += /W4 /WX /permissive- /wd4819
QMAKE_CXXFLAGS += /external:anglebrackets /external:W0
QMAKE_CXXFLAGS += /wd4005 /wd4100 /wd4127 /wd4244 /wd4251 /wd4267 /wd4275 /wd4324 /wd4702
QMAKE_LFLAGS += /INCREMENTAL:NO
LIBS += -L$$BUILDLIB

INCLUDEPATH += $$VAF_ROOT/BaseProject/Core/Foundation/include \
               $$VAF_ROOT/BaseProject/Core/Domain/include \
               $$VAF_ROOT/BaseProject/Data/Annotation/include \
               $$VAF_ROOT/BaseProject/Data/ProjectStore/include \
               $$VAF_ROOT/BaseProject/Runtime/Ipc/include \
               $$VAF_ROOT/BaseProject/Runtime/QtFoundation/include \
               $$VAF_ROOT/BaseProject/BaseModel/Models_Api/include \
               $$VAF_ROOT/BaseProject/BaseModel/Models_Common/Classify/include \
               $$VAF_ROOT/BaseProject/BaseModel/Models_Common/Detection/include \
               $$VAF_ROOT/BaseProject/AiEngine/ModelGraph/include \
               $$VAF_ROOT/BaseProject/AiEngine/Tensor/include \
               $$VAF_ROOT/BaseProject/AiEngine/Training/include \
               $$VAF_ROOT/BaseProject/AiEngine/TrainingState/include \
               $$VAF_ROOT/BaseProject/AiEngine/Export/include \
               $$VAF_ROOT/ModelPlugins/Classify/Linear/include \
               $$VAF_ROOT/ModelPlugins/Detection/Yolo11/include

defineReplace(vaf_library_output) {
    return($$BUILDLIB)
}
