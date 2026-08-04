VAF_ROOT = $$clean_path($$PWD/../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += staticlib
TARGET = vaf_foundation
DESTDIR = $$VAF_LIBRARY_DIR
SOURCES += src/Error.cpp
HEADERS += include/visionaiflow/foundation/Error.h \
           include/visionaiflow/foundation/Result.h
