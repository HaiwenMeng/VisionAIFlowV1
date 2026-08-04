VAF_ROOT = $$clean_path($$PWD/../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = app
QT += core
TARGET = VisionAIFlowCli
DESTDIR = $$VAF_BINARY_DIR
SOURCES += src/main.cpp
