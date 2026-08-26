TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += BaseProject \
    LVMs \
           ModelPlugins \
           Export \
           SoftwareProject \
           TestDemo\
           UIProject\
           VisionAIFlowApp\

BaseProject.file = BaseProject/BaseProject.pro
ModelPlugins.file = ModelPlugins/ModelPlugins.pro
Export.file = BaseProject/AiEngine/Export/Export.pro
SoftwareProject.file = SoftwareProject/SoftwareProject.pro
TestDemo.file = TestDemo/TestDemo.pro

ModelPlugins.depends = BaseProject
Export.depends = BaseProject ModelPlugins
SoftwareProject.depends = BaseProject ModelPlugins Export
TestDemo.depends = BaseProject ModelPlugins Export SoftwareProject
