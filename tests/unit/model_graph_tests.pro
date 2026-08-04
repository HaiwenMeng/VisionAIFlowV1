VAF_ROOT = $$clean_path($$PWD/../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = app
QT += core testlib
TARGET = vaf_model_graph_tests
DESTDIR = $$VAF_BINARY_DIR
SOURCES += tst_ModelGraph.cpp
PRE_TARGETDEPS += $$VAF_LIBRARY_DIR/vaf_foundation.lib \
                  $$VAF_LIBRARY_DIR/vaf_model_graph.lib
LIBS += -lvaf_foundation -lvaf_model_graph
