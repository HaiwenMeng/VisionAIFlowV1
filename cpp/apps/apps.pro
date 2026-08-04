TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += app
SUBDIRS += trainer_host
SUBDIRS += tensorrt_host
SUBDIRS += openvino_host
SUBDIRS += cli

app.file = app/app.pro
trainer_host.file = hosts/trainer/trainer_host.pro
tensorrt_host.file = hosts/tensorrt/tensorrt_host.pro
openvino_host.file = hosts/openvino/openvino_host.pro
cli.file = cli/cli.pro
