# Phase 1 进程边界

`VisionAIFlow.exe` 是 Qt Widgets 主控进程，只链接 Qt、foundation 和 IPC。它不链接 LibTorch、CUDA、TensorRT 或 OpenVINO。

`VisionTrainerHost.exe`、`VisionTensorRtHost.exe` 和 `VisionOpenVinoHost.exe` 是独立 Host 进程。当前阶段实现它们的真实启动、版本、握手、心跳和结构化停止协议；训练、TensorRT 和 OpenVINO 运行时功能会在相应依赖通过锁文件验证后分别接入各自 Host，不能加载到 UI 进程。

主控关闭时先要求用户确认，再对每个已握手 Host 发送 `shutdown`。Host 未确认完成、异常退出或非零退出码均按失败上报；不通过控制台文本推断成功，也不静默杀死训练任务。
