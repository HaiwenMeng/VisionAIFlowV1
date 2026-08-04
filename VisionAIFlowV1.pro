TEMPLATE = subdirs
CONFIG += ordered

# 按功能分组显示 qmake 子工程, 保持原有目标名称和构建入口不变。
SUBDIRS += core
SUBDIRS += data
SUBDIRS += models
SUBDIRS += ai_engine
SUBDIRS += runtime
SUBDIRS += apps
SUBDIRS += tests

core.file = cpp/core/core.pro
data.file = cpp/data/data.pro
models.file = cpp/models/models.pro
ai_engine.file = cpp/ai_engine/ai_engine.pro
runtime.file = cpp/runtime/runtime.pro
apps.file = cpp/apps/apps.pro
tests.file = tests/tests.pro

data.depends = core
models.depends = core
ai_engine.depends = core models
runtime.depends = core
apps.depends = core data models ai_engine runtime
tests.depends = core data models ai_engine runtime apps
