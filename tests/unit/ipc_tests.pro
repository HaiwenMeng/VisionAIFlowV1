VAF_ROOT = $$clean_path($$PWD/../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = app
QT += core network testlib
TARGET = vaf_ipc_tests
DESTDIR = $$VAF_BINARY_DIR
SOURCES += tst_IpcProtocol.cpp
PRE_TARGETDEPS += $$VAF_LIBRARY_DIR/vaf_foundation.lib \
                  $$VAF_LIBRARY_DIR/vaf_ipc.lib
LIBS += -lvaf_foundation -lvaf_ipc
