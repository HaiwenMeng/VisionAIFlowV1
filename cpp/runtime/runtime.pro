TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += ipc
SUBDIRS += qt_foundation

ipc.file = ipc/ipc.pro
qt_foundation.file = qt_foundation/qt_foundation.pro
qt_foundation.depends = ipc
