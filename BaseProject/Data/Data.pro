TEMPLATE = subdirs
CONFIG += ordered

# Build data libraries after the shared Core library.
SUBDIRS += ProjectStore \
           Annotation

ProjectStore.file = ProjectStore/ProjectStore.pro
Annotation.file = Annotation/Annotation.pro
Annotation.depends = ProjectStore
