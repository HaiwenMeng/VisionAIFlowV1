TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += Ipc \
           QtFoundation

Ipc.file = Ipc/Ipc.pro
QtFoundation.file = QtFoundation/QtFoundation.pro
QtFoundation.depends = Ipc
