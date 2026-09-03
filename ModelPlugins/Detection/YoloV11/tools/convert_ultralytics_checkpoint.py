#!/usr/bin/env python
"""Convert the fixed local Ultralytics 8.3.4 COCO YOLO11n checkpoint to a C++ pretraining artifact.

Run this script only with E:\\YoloProTrain\\PythonEnv11\\python.exe. The output is a parameter source only:
it must be loaded by the trainer and must never be used directly for project inference.
"""

import argparse
import json
import os
import tempfile

import torch
import ultralytics
from ultralytics import YOLO


EXPECTED_VERSION = "8.3.4"
EXPECTED_STATE_ITEMS = 499


def fail(message):
    raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser(description="Convert a local Ultralytics 8.3.4 YOLO11n checkpoint")
    parser.add_argument("--input", required=True, help="Original COCO yolo11n.pt")
    parser.add_argument("--output", required=True, help="Output pretraining artifact for YoloV11 trainer")
    args = parser.parse_args()

    if ultralytics.__version__ != EXPECTED_VERSION:
        fail("需要本地 ultralytics %s, 当前版本为 %s" % (EXPECTED_VERSION, ultralytics.__version__))
    if not os.path.isfile(args.input):
        fail("找不到输入权重: %s" % args.input)
    checkpoint = torch.load(args.input, map_location="cpu")
    if not isinstance(checkpoint, dict):
        fail("输入文件不是 Ultralytics checkpoint 字典")
    model_object = checkpoint.get("ema") or checkpoint.get("model")
    if model_object is None:
        fail("checkpoint 中缺少 model 或 ema")

    yaml_data = getattr(model_object, "yaml", {})
    if yaml_data.get("scale") != "n" or yaml_data.get("nc") != 80:
        fail("仅允许转换 COCO 80 类 YOLO11n 检测模型")
    if yaml_data.get("ch", 3) != 3:
        fail("仅支持 3 通道 YOLO11n 模型")

    state = model_object.state_dict()
    if len(state) != EXPECTED_STATE_ITEMS:
        fail("YOLO11n 参数项数量不匹配: %d" % len(state))

    model_object = model_object.float().cpu().eval()
    example = torch.zeros(1, 3, 640, 640, dtype=torch.float32)
    traced = torch.jit.trace(model_object, example, strict=False, check_trace=False)
    metadata = {
        "format": "VisionAIFlow.YoloV11.Pretrained.1",
        "ultralytics_version": ultralytics.__version__,
        "model_variant": "yolo11n",
        "input_channels": 3,
        "source_nc": 80,
        "state_dict_items": len(state),
        "source_type": "ultralytics_coco_checkpoint",
    }

    output_dir = os.path.dirname(os.path.abspath(args.output))
    if not output_dir:
        fail("输出路径没有目录")
    os.makedirs(output_dir, exist_ok=True)
    fd, temporary_path = tempfile.mkstemp(prefix="yolo11n_", suffix=".tmp", dir=output_dir)
    os.close(fd)
    try:
        torch.jit.save(traced, temporary_path, _extra_files={"visionaiflow_yolo11.json": json.dumps(metadata).encode("utf-8")})
        os.replace(temporary_path, args.output)
    finally:
        if os.path.exists(temporary_path):
            os.unlink(temporary_path)


if __name__ == "__main__":
    main()
