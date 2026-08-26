VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += dll
DEFINES += VISIONAIFLOW_PROJECT_STORE_LIBRARY
QT += core gui
TARGET = ProjectStore
DESTDIR = $$BUILDLIB
OBJECTS_DIR = $$PWD/release
MOC_DIR = $$PWD/release
RCC_DIR = $$PWD/release
UI_DIR = $$PWD/release
QMAKE_POST_LINK += copy /Y $$BUILDLIB_NATIVE\\ProjectStore.dll $$VAF_ROOT_NATIVE\\BINX64\\ProjectStore.dll
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
PRE_TARGETDEPS += $$BUILDLIB/Foundation.lib \
                  $$BUILDLIB/Domain.lib
LIBS += -lFoundation -lDomain -ladvapi32
