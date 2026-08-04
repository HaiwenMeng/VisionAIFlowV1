TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += annotation_store_tests
SUBDIRS += project_store_tests
SUBDIRS += amp_cuda_tests
SUBDIRS += export_tests
SUBDIRS += openvino_host_runtime_tests
SUBDIRS += yolo11_tests

annotation_store_tests.file = annotation_store_tests.pro
project_store_tests.file = project_store_tests.pro
amp_cuda_tests.file = amp_cuda_tests.pro
export_tests.file = export_tests.pro
openvino_host_runtime_tests.file = openvino_host_runtime_tests.pro
yolo11_tests.file = yolo11_tests.pro
