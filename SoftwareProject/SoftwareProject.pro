TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += App \
           Cli

App.file = App/App.pro
Cli.file = Cli/Cli.pro
