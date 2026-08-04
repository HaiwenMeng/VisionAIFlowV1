VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = app
QT += core network widgets
TARGET = VisionAIFlow
DESTDIR = $$VAF_BINARY_DIR
SOURCES += main.cpp \
           src/MainWindow.cpp \
           src/CreateProjectDialog.cpp \
           src/HostSupervisor.cpp \
           src/AnnotationCanvas.cpp
HEADERS += include/visionaiflow/app/MainWindow.h \
           include/visionaiflow/app/CreateProjectDialog.h \
           include/visionaiflow/app/HostSupervisor.h \
           include/visionaiflow/app/AnnotationCanvas.h
FORMS += forms/MainWindow.ui \
         forms/CreateProjectDialog.ui
PRE_TARGETDEPS += $$VAF_LIBRARY_DIR/vaf_foundation.lib \
                  $$VAF_LIBRARY_DIR/vaf_ipc.lib \
                  $$VAF_LIBRARY_DIR/vaf_project_store.lib \
                  $$VAF_LIBRARY_DIR/vaf_annotation.lib
LIBS += -lvaf_foundation -lvaf_ipc -lvaf_project_store -lvaf_annotation
