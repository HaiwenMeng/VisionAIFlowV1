# 第三方组件与许可证登记

正式交付前必须为每个组件和模型权重登记官方来源、版本、归档 SHA-256、代码许可证、权重许可证、商业分发条件、修改权、署名义务和法务结论。

当前 `config/dependencies.lock.json` 仅记录冻结版本和预期目录；因离线归档尚未提供，SHA-256 与法务审批状态均未完成，不能用于发布。

| 组件 | 冻结版本 | 法务状态 |
|---|---:|---|
| Qt | 6.9.2 | 待确认商业许可证或 LGPL 动态链接合规 |
| LibTorch | 2.7.1 cu118 | 待归档与审批 |
| TensorRT | 10.0.1.6 | 待 NVIDIA EULA 审批 |
| OpenVINO | 2026.2 | 待归档与审批 |
| ONNX / Protobuf / OpenCV / spdlog / GoogleTest / nlohmann-json | 见 lock 文件 | 待归档与审批 |
