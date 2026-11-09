TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += BaseProject \
    LVMs \
    ModelPlugins \
    UIProject \
    VisionAIFlowApp \
    LightScriptRunner

BaseProject.file = BaseProject/BaseProject.pro
LVMs.file = LVMs/LVMs.pro
ModelPlugins.file = ModelPlugins/ModelPlugins.pro

ModelPlugins.depends = BaseProject

LightScriptRunner.file = LightScriptRunner/LightScriptRunner.pro
