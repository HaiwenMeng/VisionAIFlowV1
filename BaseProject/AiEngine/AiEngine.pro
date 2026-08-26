TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += ModelGraph \
           Tensor \
           TrainingState \
           Training

ModelGraph.file = ModelGraph/ModelGraph.pro
Tensor.file = Tensor/Tensor.pro
TrainingState.file = TrainingState/TrainingState.pro
Training.file = Training/Training.pro
Export.file = Export/Export.pro

TrainingState.depends = Tensor
Training.depends = Tensor TrainingState
