TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += Core \
           Data \
           Runtime \
           BaseModel \
           AiEngine

Core.file = Core/Core.pro
Data.file = Data/Data.pro
Runtime.file = Runtime/Runtime.pro
BaseModel.file = BaseModel/BaseModel.pro
AiEngine.file = AiEngine/AiEngine.pro

Data.depends = Core
Runtime.depends = Core
BaseModel.depends = Core
AiEngine.depends = Core BaseModel
