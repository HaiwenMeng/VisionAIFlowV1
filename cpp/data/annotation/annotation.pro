VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += staticlib
QT += core
TARGET = vaf_annotation
DESTDIR = $$VAF_LIBRARY_DIR
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
PRE_TARGETDEPS += $$VAF_LIBRARY_DIR/vaf_foundation.lib
LIBS += -lvaf_foundation
