TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += AnnotationStore_Tests \
           ProjectStore_Tests \
           AmpCuda_Tests \
           Export_Tests \
           Yolo11_Tests

AnnotationStore_Tests.file = AnnotationStore_Tests/AnnotationStore_Tests.pro
ProjectStore_Tests.file = ProjectStore_Tests/ProjectStore_Tests.pro
AmpCuda_Tests.file = AmpCuda_Tests/AmpCuda_Tests.pro
Export_Tests.file = Export_Tests/Export_Tests.pro
Yolo11_Tests.file = Yolo11_Tests/Yolo11_Tests.pro
