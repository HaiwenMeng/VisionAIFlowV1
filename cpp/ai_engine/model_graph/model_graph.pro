VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += staticlib
QT += core
TARGET = vaf_model_graph
DESTDIR = $$VAF_LIBRARY_DIR
SOURCES += src/ModelGraph.cpp
HEADERS += include/visionaiflow/model_graph/ModelGraph.h
PRE_TARGETDEPS += $$VAF_LIBRARY_DIR/vaf_foundation.lib
LIBS += -lvaf_foundation
