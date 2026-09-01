VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = app
QT += core testlib
TARGET = vaf_annotation_store_tests
DESTDIR = $$VAF_BINARY_DIR
SOURCES += tst_AnnotationStore.cpp
PRE_TARGETDEPS += $$BUILDLIB/VisionAIFlowCore.lib \
                  $$BUILDLIB/Annotation.lib \
                  $$BUILDLIB/ProjectStore.lib
LIBS += -lVisionAIFlowCore -lAnnotation -lProjectStore -ladvapi32
