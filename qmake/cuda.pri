CUDA_ROOT = C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.8
CUDA_QMAKE_ROOT = C:/PROGRA~1/NVIDIA~2/CUDA/v11.8
CUDA_NVCC = $$CUDA_QMAKE_ROOT/bin/nvcc.exe
CUDA_HOST_COMPILER = "D:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Tools/MSVC/14.29.30133/bin/Hostx64/x64"

!exists($$CUDA_NVCC): error(CUDA 11.8 nvcc is missing at $$CUDA_NVCC)

cuda_compiler.name = CUDA ${QMAKE_FILE_IN}
cuda_compiler.input = CUDA_SOURCES
cuda_compiler.output = $$OBJECTS_DIR/${QMAKE_FILE_BASE}_cuda.obj
cuda_compiler.variable_out = OBJECTS
cuda_compiler.CONFIG += no_link target_predeps
cuda_compiler.commands = cd /d $$VAF_CUDA_WORKING_DIRECTORY && $$CUDA_NVCC --use-local-env -ccbin $$CUDA_HOST_COMPILER -c -std=c++17 -gencode arch=compute_75,code=sm_75 -Xcompiler /EHsc,/MD -I$$VAF_ROOT/cpp/core/foundation/include -I$$VAF_ROOT/cpp/apps/hosts/trainer/include -I$$CUDA_QMAKE_ROOT/include -o ${QMAKE_FILE_OUT} ${QMAKE_FILE_IN}
QMAKE_EXTRA_COMPILERS += cuda_compiler

LIBS += -L$$CUDA_QMAKE_ROOT/lib/x64 -lcudart -lcuda
