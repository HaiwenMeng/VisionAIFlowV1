VAF_ROOT = $$clean_path($$PWD/../../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += staticlib
QT += core
TARGET = vaf_classification_common
DESTDIR = $$VAF_LIBRARY_DIR
INCLUDEPATH += $$VAF_ROOT/cpp/models/api/include \
               $$VAF_ROOT/cpp/models/classification/common/include
SOURCES += src/ClassificationModelAdapter.cpp
HEADERS += include/visionaiflow/models/classification/IClassificationModelAdapter.h
PRE_TARGETDEPS += $$VAF_LIBRARY_DIR/vaf_models_api.lib
LIBS += -lvaf_models_api
