TEMPLATE = lib
CONFIG += dll release c++20
CONFIG -= debug debug_and_release staticlib warn_on

QT += core gui widgets

TARGET = BaseAnnoDisp
DESTDIR = $$clean_path($$PWD/../../BuildLib)
OBJECTS_DIR = $$PWD/release
MOC_DIR = $$PWD/release
RCC_DIR = $$PWD/release
UI_DIR = $$PWD/release

win32-msvc*: QMAKE_CXXFLAGS += /utf-8 /W4 /WX /permissive- /wd4251 /wd4275

DEFINES += BASE_ANNODISP_LIBRARY

SOURCES += \
    BaseAnnoDisplayWidget.cpp

HEADERS += \
    BaseAnnoDispExport.h \
    BaseAnnoDisplayTypes.h \
    BaseAnnoDisplayWidget.h
