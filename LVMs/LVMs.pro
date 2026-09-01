TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += \
    TrtSAM2Lib \
    TrtSAM3Lib

TrtSAM2Lib.file = TrtSAM2Lib/TrtSAM2Lib.pro
TrtSAM3Lib.file = TrtSAM3Lib/TrtSAM3Lib.pro
