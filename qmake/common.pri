isEmpty(VAF_ROOT): error(VAF_ROOT must be set by the including .pro file)

VAF_DEPS_ROOT = F:/VisionAIFlowDeps
VAF_QT_ROOT = F:/Qt6.7.3/6.7.3/msvc2019_64
VAF_DEPS_RELEASE_ROOT = $$VAF_DEPS_ROOT/install/Release

CONFIG += release c++20 no_utf8_source
CONFIG -= debug debug_and_release staticlib warn_on
!contains(CONFIG, release): error(Only the Release configuration is supported.)

!exists($$VAF_QT_ROOT/bin/qmake.exe): error(Qt 6.7.3 qmake is missing at $$VAF_QT_ROOT/bin/qmake.exe)

DESTDIR = $$VAF_ROOT/BINX64
BUILDLIB = $$VAF_ROOT/BuildLib
VAF_MODELS_DIR = $$DESTDIR/model
VAF_AI_MODEL_PLUGINS_DIR = $$DESTDIR/AIModelPlugins
VAF_LIBRARY_DIR = $$BUILDLIB
VAF_BINARY_DIR = $$DESTDIR
VAF_ROOT_NATIVE = $$replace(VAF_ROOT, /, \\\\)
BUILDLIB_NATIVE = $$replace(BUILDLIB, /, \\\\)

QMAKE_CXXFLAGS += /W4 /WX /permissive- /wd4819
QMAKE_CXXFLAGS += /external:anglebrackets /external:W0
QMAKE_CXXFLAGS += /wd4005 /wd4100 /wd4127 /wd4244 /wd4251 /wd4267 /wd4275 /wd4324 /wd4702
QMAKE_LFLAGS += /INCREMENTAL:NO
LIBS += -L$$BUILDLIB

INCLUDEPATH += $$VAF_ROOT/BaseProject/Core/include \
                $$VAF_ROOT/BaseProject/Data/Annotation/include \
               $$VAF_ROOT/BaseProject/Data/ProjectStore/include \
               $$VAF_ROOT/BaseProject/Runtime/Ipc/include \
               $$VAF_ROOT/BaseProject/Runtime/QtFoundation/include \
               $$VAF_ROOT/BaseProject/PluginApi/include

defineReplace(vaf_library_output) {
    return($$BUILDLIB)
}
