TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += Models_Api \
           Models_Common

Models_Api.file = Models_Api/Models_Api.pro
Models_Common.file = Models_Common/Models_Common.pro

Models_Common.depends = Models_Api
