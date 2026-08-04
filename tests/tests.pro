TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += unit
SUBDIRS += integration

unit.file = unit/unit.pro
integration.file = integration/integration.pro
