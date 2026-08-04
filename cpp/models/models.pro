TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += models_common
SUBDIRS += yolo11

models_common.file = detection/common/models_common.pro
yolo11.file = detection/yolo11/yolo11.pro
yolo11.depends = models_common
