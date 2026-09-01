VAF_ROOT = $$clean_path($$PWD/../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += dll
QT += core
TARGET = PluginApi
DEFINES += VISIONAIFLOW_PLUGIN_API_LIBRARY
QMAKE_CXXFLAGS += /utf-8
DESTDIR = $$BUILDLIB
OBJECTS_DIR = $$PWD/release
MOC_DIR = $$PWD/release
RCC_DIR = $$PWD/release
UI_DIR = $$PWD/release
QMAKE_POST_LINK += copy /Y $$BUILDLIB_NATIVE\\PluginApi.dll $$VAF_ROOT_NATIVE\\BINX64\\PluginApi.dll
SOURCES += src/PluginManager.cpp
HEADERS += include/visionaiflow/plugin_api/PluginApi.h \
           include/visionaiflow/plugin_api/PluginManager.h
