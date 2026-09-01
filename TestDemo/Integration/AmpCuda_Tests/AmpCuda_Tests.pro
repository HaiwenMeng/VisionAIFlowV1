VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)
include($$VAF_ROOT/qmake/libtorch_cuda.pri)

TEMPLATE = app
QT += core testlib
TARGET = vaf_amp_cuda_tests
DESTDIR = $$VAF_BINARY_DIR
DEFINES += USE_CUDA
QMAKE_CXXFLAGS += /external:W0 /external:I$$VAF_TORCH_ROOT/include /wd4100 /wd4127 /wd4244 /wd4251 /wd4267 /wd4275 /wd4702
SOURCES += tst_AmpControllerCuda.cpp
PRE_TARGETDEPS += $$BUILDLIB/VisionAIFlowCore.lib \
                  $$BUILDLIB/Training.lib
LIBS += -lVisionAIFlowCore -lTraining -lcuda
