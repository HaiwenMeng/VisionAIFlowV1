TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += annotation
SUBDIRS += project_store

annotation.file = annotation/annotation.pro
project_store.file = project_store/project_store.pro
