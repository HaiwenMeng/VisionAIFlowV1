VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += dll
DEFINES += VISIONAIFLOW_MODELS_API_LIBRARY
QT += core
TARGET = Models_Api
DESTDIR = $$BUILDLIB
OBJECTS_DIR = $$PWD/release
MOC_DIR = $$PWD/release
RCC_DIR = $$PWD/release
UI_DIR = $$PWD/release
QMAKE_POST_LINK += copy /Y $$BUILDLIB_NATIVE\\Models_Api.dll $$VAF_ROOT_NATIVE\\BINX64\\Models_Api.dll
SOURCES += src/ModelDescriptor.cpp \
           src/ModelRegistry.cpp
HEADERS += include/visionaiflow/models/api/ModelCapability.h \
           include/visionaiflow/models/api/ModelSignature.h \
           include/visionaiflow/models/api/ModelDescriptor.h \
           include/visionaiflow/models/api/ModelRequests.h \
           include/visionaiflow/models/api/IModelAdapter.h \
           include/visionaiflow/models/api/ModelRegistry.h
PRE_TARGETDEPS += $$BUILDLIB/Foundation.lib \
                  $$BUILDLIB/Domain.lib
LIBS += -lFoundation -lDomain
