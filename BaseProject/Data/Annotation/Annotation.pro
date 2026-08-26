VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += dll
DEFINES += VISIONAIFLOW_ANNOTATION_LIBRARY
QT += core
TARGET = Annotation
DESTDIR = $$BUILDLIB
OBJECTS_DIR = $$PWD/release
MOC_DIR = $$PWD/release
RCC_DIR = $$PWD/release
UI_DIR = $$PWD/release
QMAKE_POST_LINK += copy /Y $$BUILDLIB_NATIVE\\Annotation.dll $$VAF_ROOT_NATIVE\\BINX64\\Annotation.dll
SOURCES += src/Geometry.cpp \
           src/AnnotationStore.cpp \
           src/AnnotationDocument.cpp \
           src/LabelImpactAnalyzer.cpp \
           src/LabelMutationTransaction.cpp
HEADERS += include/visionaiflow/annotation/Geometry.h \
           include/visionaiflow/annotation/AnnotationStore.h \
           include/visionaiflow/annotation/AnnotationDocument.h \
           include/visionaiflow/annotation/LabelImpactAnalyzer.h \
           include/visionaiflow/annotation/LabelMutationTransaction.h
PRE_TARGETDEPS += $$BUILDLIB/Foundation.lib \
                  $$BUILDLIB/ProjectStore.lib
LIBS += -lFoundation -lProjectStore
