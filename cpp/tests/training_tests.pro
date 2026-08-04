VAF_ROOT = $$clean_path($$PWD/../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = app
QT += core testlib
TARGET = vaf_training_tests
DESTDIR = $$VAF_BINARY_DIR
QMAKE_CXXFLAGS += /external:W0 /external:I$$VAF_TORCH_ROOT/include /wd4100 /wd4127 /wd4244 /wd4251 /wd4267 /wd4275 /wd4702
SOURCES += unit/tst_LinearClassifierTrainer.cpp
INCLUDEPATH += $$VAF_TORCH_ROOT/include \
               $$VAF_TORCH_ROOT/include/torch/csrc/api/include
PRE_TARGETDEPS += $$VAF_LIBRARY_DIR/vaf_foundation.lib \
                  $$VAF_LIBRARY_DIR/vaf_models_common.lib \
                  $$VAF_LIBRARY_DIR/vaf_training.lib
LIBS += -lvaf_foundation -lvaf_models_common -lvaf_training -L$$VAF_TORCH_ROOT/lib -ltorch_cpu -lc10
