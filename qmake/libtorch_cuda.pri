VAF_CUDA_ROOT = C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.8
VAF_CUDA_QMAKE_ROOT = C:/PROGRA~1/NVIDIA~2/CUDA/v11.8
VAF_TORCH_ROOT = F:/VisionAIFlowDeps/libtorch/2.7.1-cu118/release

!exists($$VAF_CUDA_ROOT/bin/nvcc.exe): error(CUDA 11.8 nvcc is missing at $$VAF_CUDA_ROOT/bin/nvcc.exe)
!exists($$VAF_TORCH_ROOT/include/torch/csrc/api/include/torch/version.h): error(LibTorch 2.7.1 cu118 Release headers are missing at $$VAF_TORCH_ROOT)

INCLUDEPATH += $$VAF_TORCH_ROOT/include \
               $$VAF_TORCH_ROOT/include/torch/csrc/api/include \
               $$VAF_CUDA_QMAKE_ROOT/include

LIBS += -L$$VAF_CUDA_QMAKE_ROOT/lib/x64 -lcudart \
        -L$$VAF_TORCH_ROOT/lib -ltorch -ltorch_cpu -ltorch_cuda -lc10 -lc10_cuda
