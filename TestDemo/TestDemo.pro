TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += Unit \
           Integration

Unit.file = Unit/Unit.pro
Integration.file = Integration/Integration.pro
