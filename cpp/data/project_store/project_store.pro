VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += staticlib
QT += core gui
TARGET = vaf_project_store
DESTDIR = $$VAF_LIBRARY_DIR
SOURCES += src/ProjectDefinition.cpp \
           src/ProjectStore.cpp \
           src/DatasetIndex.cpp \
           src/ProjectLock.cpp \
           src/ProjectMigration.cpp \
           src/LabelStore.cpp
HEADERS += include/visionaiflow/project_store/ProjectDefinition.h \
           include/visionaiflow/project_store/ProjectStore.h \
           include/visionaiflow/project_store/DatasetIndex.h \
           include/visionaiflow/project_store/ProjectLock.h \
           include/visionaiflow/project_store/ProjectMigration.h \
           include/visionaiflow/project_store/LabelStore.h
PRE_TARGETDEPS += $$VAF_LIBRARY_DIR/vaf_foundation.lib
QMAKE_PRL_LIBS += -ladvapi32
LIBS += -lvaf_foundation
