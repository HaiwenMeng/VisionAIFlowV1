VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = app
QT += core testlib
TARGET = AnnotationHistory_Tests
OBJECTS_DIR = $$PWD/release
DESTDIR = $$VAF_BINARY_DIR
SOURCES += tst_AnnotationDocument.cpp
PRE_TARGETDEPS += $$BUILDLIB/VisionAIFlowCore.lib \
                  $$BUILDLIB/Annotation.lib
LIBS += -lVisionAIFlowCore -lAnnotation
