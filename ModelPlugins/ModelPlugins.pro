TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += Classify \
           Detection

Classify.file = Classify/Classify.pro
Detection.file = Detection/Detection.pro
