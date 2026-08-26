TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += models_api
SUBDIRS += classification_common
SUBDIRS += linear_classification
SUBDIRS += models_common
SUBDIRS += yolo11
SUBDIRS += builtins

models_api.file = api/models_api.pro
classification_common.file = classification/common/classification_common.pro
linear_classification.file = classification/linear/linear_classification.pro
models_common.file = detection/common/models_common.pro
yolo11.file = detection/yolo11/yolo11.pro
builtins.file = builtins/models_builtins.pro
classification_common.depends = models_api
linear_classification.depends = classification_common models_api
models_common.depends = models_api
yolo11.depends = models_common
builtins.depends = models_api linear_classification yolo11
