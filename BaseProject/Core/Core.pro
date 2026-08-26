TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += Foundation \
           Domain

Foundation.file = Foundation/Foundation.pro
Domain.file = Domain/Domain.pro
Domain.depends = Foundation
