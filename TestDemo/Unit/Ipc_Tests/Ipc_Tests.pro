VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = app
QT += core network testlib
TARGET = Ipc_Tests
OBJECTS_DIR = $$PWD/release
DESTDIR = $$VAF_BINARY_DIR
SOURCES += tst_IpcProtocol.cpp
PRE_TARGETDEPS += $$BUILDLIB/VisionAIFlowCore.lib \
                  $$BUILDLIB/Ipc.lib
LIBS += -lVisionAIFlowCore -lIpc
