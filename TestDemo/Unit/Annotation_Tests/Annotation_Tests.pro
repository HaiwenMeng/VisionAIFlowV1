VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = app
QT += core testlib
TARGET = Annotation_Tests
OBJECTS_DIR = $$PWD/release
DESTDIR = $$VAF_BINARY_DIR
SOURCES += tst_Geometry.cpp
PRE_TARGETDEPS += $$BUILDLIB/Foundation.lib \
                  $$BUILDLIB/Annotation.lib
LIBS += -lFoundation -lAnnotation
