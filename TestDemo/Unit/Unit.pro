TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += Ipc_Tests \
           Domain_Tests \
           Annotation_Tests \
           AnnotationHistory_Tests \
           TensorDataLoader_Tests \
           Amp_Tests

Ipc_Tests.file = Ipc_Tests/Ipc_Tests.pro
Domain_Tests.file = Domain_Tests/Domain_Tests.pro
Annotation_Tests.file = Annotation_Tests/Annotation_Tests.pro
AnnotationHistory_Tests.file = AnnotationHistory_Tests/AnnotationHistory_Tests.pro
TensorDataLoader_Tests.file = TensorDataLoader_Tests/TensorDataLoader_Tests.pro
Amp_Tests.file = Amp_Tests/Amp_Tests.pro
