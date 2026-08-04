VAF_ROOT = $$clean_path($$PWD/../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = app
QT += core testlib
TARGET = vaf_annotation_store_tests
DESTDIR = $$VAF_BINARY_DIR
SOURCES += tst_AnnotationStore.cpp
PRE_TARGETDEPS += $$VAF_LIBRARY_DIR/vaf_foundation.lib \
                  $$VAF_LIBRARY_DIR/vaf_annotation.lib \
                  $$VAF_LIBRARY_DIR/vaf_project_store.lib
LIBS += -lvaf_foundation -lvaf_annotation -lvaf_project_store -ladvapi32
