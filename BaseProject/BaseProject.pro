TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += Core \
           Data \
           Runtime \
           PluginApi

Core.file = Core/Core.pro
Data.file = Data/Data.pro
Runtime.file = Runtime/Runtime.pro
PluginApi.file = PluginApi/PluginApi.pro

Data.depends = Core
Runtime.depends = Core
PluginApi.depends = Core
