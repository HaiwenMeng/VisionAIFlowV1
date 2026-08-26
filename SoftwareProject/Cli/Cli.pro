VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = app
QT += core
TARGET = Cli
DESTDIR = $$VAF_BINARY_DIR
OBJECTS_DIR = $$PWD/release
SOURCES += src/main.cpp
