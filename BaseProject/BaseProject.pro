TEMPLATE = subdirs
CONFIG += ordered

# Build the shared libraries in dependency order.
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
