VAF_ROOT = $$clean_path($$PWD/../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = app
QT += core testlib
TARGET = vaf_annotation_history_tests
DESTDIR = $$VAF_BINARY_DIR
SOURCES += unit/tst_AnnotationDocument.cpp
PRE_TARGETDEPS += $$VAF_LIBRARY_DIR/vaf_foundation.lib \
                  $$VAF_LIBRARY_DIR/vaf_annotation.lib
LIBS += -lvaf_foundation -lvaf_annotation
