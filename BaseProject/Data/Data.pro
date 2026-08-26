TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += ProjectStore \
           Annotation

ProjectStore.file = ProjectStore/ProjectStore.pro
Annotation.file = Annotation/Annotation.pro
Annotation.depends = ProjectStore
