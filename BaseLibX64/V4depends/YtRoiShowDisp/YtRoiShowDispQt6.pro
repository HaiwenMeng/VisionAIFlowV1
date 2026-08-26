QT += core gui widgets uitools

TEMPLATE = lib
TARGET = YtRoiShowDispQt6
CONFIG += staticlib c++17 release
CONFIG -= debug_and_release debug
DEFINES += QDESIGNER_EXPORT_WIDGETS

DESTDIR = $$PWD/release
OBJECTS_DIR = $$PWD/release
MOC_DIR = $$PWD/release
RCC_DIR = $$PWD/release
UI_DIR = $$PWD/release

INCLUDEPATH += $$PWD \
               $$PWD/ROIItem \
               $$PWD/../YtVisionDefine

SOURCES += \
    BaseItem.cpp \
    ControlItem.cpp \
    QGScene.cpp \
    QGView.cpp \
    ytroishowdisp.cpp \
    ROIItem/QtArcROI.cpp \
    ROIItem/QtCircleROI.cpp \
    ROIItem/QtEllipseROI.cpp \
    ROIItem/QtLineROI.cpp \
    ROIItem/QtLineROISeg.cpp \
    ROIItem/QtPieROI.cpp \
    ROIItem/QtPointROI.cpp \
    ROIItem/QtPolygonROI.cpp \
    ROIItem/QtPolygonROIE.cpp \
    ROIItem/QtRectROI.cpp \
    ROIItem/QtRingROI.cpp \
    ROIItem/QtRotateRectROI.cpp \
    ROIItem/QtWaistShapeROI.cpp

HEADERS += \
    BaseItem.h \
    ControlItem.h \
    QGScene.h \
    QGView.h \
    ytroishowdisp.h \
    ROIItem/QtArcROI.h \
    ROIItem/QtCircleROI.h \
    ROIItem/QtEllipseROI.h \
    ROIItem/QtLineROI.h \
    ROIItem/QtLineROISeg.h \
    ROIItem/QtPieROI.h \
    ROIItem/QtPointROI.h \
    ROIItem/QtPolygonROI.h \
    ROIItem/QtPolygonROIE.h \
    ROIItem/QtRectROI.h \
    ROIItem/QtRingROI.h \
    ROIItem/QtRotateRectROI.h \
    ROIItem/QtWaistShapeROI.h

FORMS += ytroishowdisp.ui
RESOURCES += icons.qrc
