VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = app
QT += core gui testlib
TARGET = vaf_project_store_tests
DESTDIR = $$VAF_BINARY_DIR
SOURCES += tst_ProjectStore.cpp
PRE_TARGETDEPS += $$BUILDLIB/Foundation.lib \
                  $$BUILDLIB/Domain.lib \
                  $$BUILDLIB/ProjectStore.lib
LIBS += -lFoundation -lDomain -lProjectStore -ladvapi32
