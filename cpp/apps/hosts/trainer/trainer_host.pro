VAF_ROOT = $$clean_path($$PWD/../../../..)
include($$VAF_ROOT/qmake/common.pri)
OBJECTS_DIR = $$VAF_OUTPUT_ROOT/trainer_host_obj
VAF_CUDA_WORKING_DIRECTORY = $$VAF_ROOT/out/qmake/$$VAF_CONFIGURATION/cpp/apps/hosts/trainer
include($$VAF_ROOT/qmake/cuda.pri)
CONFIG(debug, debug|release) {
    QMAKE_CXX = cmd.exe /d /s /c call $$VAF_ROOT/tools/qmake/trainer-cl-debug.cmd
    QMAKE_LINK = cmd.exe /d /s /c call $$VAF_ROOT/tools/qmake/trainer-link-debug.cmd
} else {
    QMAKE_CXX = cmd.exe /d /s /c call $$VAF_ROOT/tools/qmake/trainer-cl-release.cmd
    QMAKE_LINK = cmd.exe /d /s /c call $$VAF_ROOT/tools/qmake/trainer-link-release.cmd
}

TEMPLATE = app
QT += core network
TARGET = VisionTrainerHost
DESTDIR = $$VAF_BINARY_DIR
SOURCES += src/main.cpp \
           src/CudaRngCheckpoint.cpp \
           src/TrainerJobCoordinator.cpp
CUDA_SOURCES += src/CudaRuntimeProbe.cu
HEADERS += include/visionaiflow/trainer_host/CudaRuntimeProbe.h \
           include/visionaiflow/trainer_host/CudaRngCheckpoint.h \
           include/visionaiflow/trainer_host/TrainerJobCoordinator.h
PRE_TARGETDEPS += $$VAF_LIBRARY_DIR/vaf_foundation.lib \
                  $$VAF_LIBRARY_DIR/vaf_ipc.lib \
                  $$VAF_LIBRARY_DIR/vaf_qt_foundation.lib \
                  $$VAF_LIBRARY_DIR/vaf_models_common.lib \
                  $$VAF_LIBRARY_DIR/vaf_model_graph.lib \
                  $$VAF_LIBRARY_DIR/vaf_tensor.lib \
                  $$VAF_LIBRARY_DIR/vaf_training.lib
INCLUDEPATH += $$VAF_TORCH_ROOT/include \
               $$VAF_TORCH_ROOT/include/torch/csrc/api/include \
               $$VAF_CUDA_QMAKE_ROOT/include \
               $$VAF_ROOT/cpp/apps/hosts/trainer/include
LIBS += -lvaf_foundation -lvaf_ipc -lvaf_qt_foundation -lvaf_models_common -lvaf_model_graph -lvaf_tensor -lvaf_training \
        -L$$VAF_TORCH_ROOT/lib -ltorch -ltorch_cpu -ltorch_cuda -lc10 -lc10_cuda
