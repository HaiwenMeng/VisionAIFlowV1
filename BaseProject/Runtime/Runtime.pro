TEMPLATE = subdirs
CONFIG += ordered

# Build runtime libraries after the shared Core library.
SUBDIRS += Ipc \
           QtFoundation

Ipc.file = Ipc/Ipc.pro
QtFoundation.file = QtFoundation/QtFoundation.pro
QtFoundation.depends = Ipc
