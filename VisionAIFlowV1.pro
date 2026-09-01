TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += BaseProject \
    LVMs \
           ModelPlugins \
           UIProject\
           VisionAIFlowApp\

BaseProject.file = BaseProject/BaseProject.pro
LVMs.file = LVMs/LVMs.pro
ModelPlugins.file = ModelPlugins/ModelPlugins.pro

ModelPlugins.depends = BaseProject
