TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += model_graph
SUBDIRS += tensor
SUBDIRS += training
SUBDIRS += export

model_graph.file = model_graph/model_graph.pro
tensor.file = tensor/tensor.pro
training.file = training/training.pro
export.file = export/export.pro
training.depends = model_graph tensor
export.depends = model_graph training
