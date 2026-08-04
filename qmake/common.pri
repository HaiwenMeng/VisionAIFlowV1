isEmpty(VAF_ROOT): error(VAF_ROOT must be set by the including .pro file)

VAF_DEPS_ROOT = F:/VisionAIFlowDeps
VAF_QT_ROOT = F:/Qt6.7.3/6.7.3/msvc2019_64
VAF_CUDA_ROOT = C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.8
VAF_CUDA_QMAKE_ROOT = C:/PROGRA~1/NVIDIA~2/CUDA/v11.8
VAF_TENSORRT_ROOT = E:/TensorRT-10.0.1.6
VAF_OPENVINO_ROOT = F:/VisionAIFlowDeps/openvino2025.3.0

CONFIG(release, debug|release) {
    VAF_CONFIGURATION = Release
    VAF_TORCH_CONFIGURATION = release
} else {
    VAF_CONFIGURATION = Debug
    VAF_TORCH_CONFIGURATION = debug
}

VAF_TORCH_ROOT = F:/VisionAIFlowDeps/libtorch/2.7.1-cu118/$$VAF_TORCH_CONFIGURATION

!exists($$VAF_QT_ROOT/bin/qmake.exe): error(Qt 6.7.3 qmake is missing at $$VAF_QT_ROOT/bin/qmake.exe)
!exists($$VAF_CUDA_ROOT/bin/nvcc.exe): error(CUDA 11.8 nvcc is missing at $$VAF_CUDA_ROOT/bin/nvcc.exe)
!exists($$VAF_TENSORRT_ROOT/include/NvInferVersion.h): error(TensorRT 10.0.1.6 headers are missing at $$VAF_TENSORRT_ROOT)
!exists($$VAF_OPENVINO_ROOT/include/openvino/openvino.hpp): error(OpenVINO 2025.3.0 headers are missing at $$VAF_OPENVINO_ROOT)
!exists($$VAF_TORCH_ROOT/include/torch/csrc/api/include/torch/version.h): error(LibTorch 2.7.1 cu118 $$VAF_CONFIGURATION headers are missing at $$VAF_TORCH_ROOT)

VAF_OUTPUT_ROOT = $$VAF_ROOT/out/qmake/$$VAF_CONFIGURATION
VAF_LIBRARY_DIR = $$VAF_OUTPUT_ROOT/lib
VAF_BINARY_DIR = $$VAF_OUTPUT_ROOT/bin

CONFIG += c++20 no_utf8_source
CONFIG -= warn_on
QMAKE_CXXFLAGS += /W4 /WX /permissive- /wd4819
# Keep /WX for VisionAIFlow code. These diagnostics originate in the pinned
# LibTorch 2.7.1 public templates when instantiated by MSVC 19.29.
QMAKE_CXXFLAGS += /external:anglebrackets /external:W0
QMAKE_CXXFLAGS += /wd4005 /wd4100 /wd4127 /wd4244 /wd4251 /wd4267 /wd4275 /wd4324 /wd4702
QMAKE_LFLAGS += /INCREMENTAL:NO

INCLUDEPATH += $$VAF_ROOT/cpp/core/foundation/include \
               $$VAF_ROOT/cpp/core/domain/include \
               $$VAF_ROOT/cpp/data/annotation/include \
               $$VAF_ROOT/cpp/data/project_store/include \
               $$VAF_ROOT/cpp/ai_engine/model_graph/include \
               $$VAF_ROOT/cpp/ai_engine/tensor/include \
               $$VAF_ROOT/cpp/ai_engine/training/include \
               $$VAF_ROOT/cpp/ai_engine/export/include \
               $$VAF_ROOT/cpp/models/detection/common/include \
               $$VAF_ROOT/cpp/models/detection/yolo11/include \
               $$VAF_ROOT/cpp/runtime/ipc/include \
               $$VAF_ROOT/cpp/runtime/qt_foundation/include \
               $$VAF_ROOT/cpp/apps/app/include

LIBS += -L$$VAF_LIBRARY_DIR
