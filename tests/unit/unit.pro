TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += ipc_tests
SUBDIRS += domain_tests
SUBDIRS += annotation_tests
SUBDIRS += annotation_history_tests
SUBDIRS += tensor_tests
SUBDIRS += model_graph_tests
SUBDIRS += training_tests
SUBDIRS += tensor_dataloader_tests
SUBDIRS += amp_tests

ipc_tests.file = ipc_tests.pro
domain_tests.file = domain_tests.pro
annotation_tests.file = annotation_tests.pro
annotation_history_tests.file = annotation_history_tests.pro
tensor_tests.file = tensor_tests.pro
model_graph_tests.file = model_graph_tests.pro
training_tests.file = training_tests.pro
tensor_dataloader_tests.file = tensor_dataloader_tests.pro
amp_tests.file = amp_tests.pro
