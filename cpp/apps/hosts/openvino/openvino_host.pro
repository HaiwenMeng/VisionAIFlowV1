VAF_ROOT = $$clean_path($$PWD/../../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = app
QT += core network
TARGET = VisionOpenVinoHost
DESTDIR = $$VAF_BINARY_DIR
QMAKE_CXXFLAGS += /external:W0 /external:I$$VAF_OPENVINO_ROOT/include /wd4996
SOURCES += src/main.cpp
SOURCES += src/OpenVinoClassifier.cpp
HEADERS += include/visionaiflow/openvino_host/OpenVinoClassifier.h
PRE_TARGETDEPS += $$VAF_LIBRARY_DIR/vaf_foundation.lib \
                  $$VAF_LIBRARY_DIR/vaf_ipc.lib \
                  $$VAF_LIBRARY_DIR/vaf_qt_foundation.lib \
                  $$VAF_LIBRARY_DIR/vaf_models_common.lib \
                  $$VAF_LIBRARY_DIR/vaf_yolo11.lib
INCLUDEPATH += $$VAF_OPENVINO_ROOT/include \
               $$VAF_ROOT/cpp/apps/hosts/openvino/include
LIBS += -lvaf_foundation -lvaf_ipc -lvaf_qt_foundation -lvaf_models_common -lvaf_yolo11 \
        -L$$VAF_OPENVINO_ROOT/lib -lopenvino
