VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += dll
DEFINES += VISIONAIFLOW_MODEL_GRAPH_LIBRARY
QT += core
TARGET = ModelGraph
DESTDIR = $$BUILDLIB
OBJECTS_DIR = $$PWD/release
MOC_DIR = $$PWD/release
RCC_DIR = $$PWD/release
UI_DIR = $$PWD/release
QMAKE_POST_LINK += copy /Y $$BUILDLIB_NATIVE\\ModelGraph.dll $$VAF_ROOT_NATIVE\\BINX64\\ModelGraph.dll
SOURCES += src/ModelGraph.cpp
HEADERS += include/visionaiflow/model_graph/ModelGraph.h
PRE_TARGETDEPS += $$BUILDLIB/Foundation.lib
LIBS += -lFoundation
