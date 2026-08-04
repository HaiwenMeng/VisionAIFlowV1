TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += foundation
SUBDIRS += domain

foundation.file = foundation/foundation.pro
domain.file = domain/domain.pro
domain.depends = foundation
