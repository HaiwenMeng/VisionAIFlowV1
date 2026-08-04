# VisionAIFlowV1 Qt/C++ + LibTorch 重构执行规范

> 文档状态：当前实施基线（Source of Truth）  
> 冻结日期：2026-07-21  
> 新项目根目录：`F:\VisionAIFlowV1`  
> 外部依赖根目录：`F:\VisionAIFlowDeps`  
> 目标读者：在新 Codex 任务中直接执行重构的开发 Agent

## 1. 文档用途与优先级

本文档是 `VisionAIFlowV1` 新仓库的实施规范、构建手册和验收合同。新 Codex 任务应先完整读取项目根目录的 `AGENTS.md`，再完整读取本文档，然后用中文建立执行计划并按阶段实施。

本文档明确取代此前“Qt/C++ 主程序 + Python 训练引擎”的候选路线。旧目录中的文档、Python 原型、JSON Lines `EngineClient` 和 TensorRT 10.10 候选版本均不是本项目的实施依据。

以下路径只能作为只读需求参考，不得原地修改、移动或删除：

- `F:\ytprojectv2alln\DeepLSoftProject\YtAIFlowV1`
- `F:\ytprojectv2alln\DeepLSoftProject\YtYoloProTrainGrayV2`
- `F:\VisionAIFlow`

所有新代码、文档、构建输出和 Git 操作仅作用于：

```text
F:\VisionAIFlowV1
```

如代码、运行结果、项目规则和本文档发生冲突，执行顺序为：

1. 当前系统和用户指令。
2. `F:\VisionAIFlowV1\AGENTS.md`。
3. 经测试验证的当前代码和运行事实。
4. 本文档。
5. 旧项目和旧文档，仅作为参考证据。

不得通过假实现、空实现、Mock 结果、静默降级或硬编码成功状态让阶段验收通过。任何依赖、模型、文件、Runtime、IPC 或设备错误都必须返回明确的 `errorCode`、`errorMessage`，并写入日志。

## 2. 已冻结的产品决策

### 2.1 产品边界

- Windows x64 离线视觉标注、训练、验证、导出和推理软件。
- 最低操作系统为 Windows 10 x64，同时支持 Windows 11 x64。
- NVIDIA GPU 最低为 RTX 20 系列，即最低计算能力 SM 7.5。
- 只支持单卡训练，不实现多 GPU 或多机训练。
- GPU 推理使用 TensorRT。
- CPU 推理使用 OpenVINO，最低 CPU 指令集按 AVX2 设计。
- 客户现场必须可以完全离线训练和推理。
- 客户安装目录不包含自研 `.py` 文件；本项目产品代码不依赖 Python 运行环境。
- 项目数据使用本地文件夹、JSON 和 JSONL，不使用 SQL 数据库。
- 项目创建时必须固定项目类型，之后不可直接修改；转换类型必须创建新项目。
- 基础程序、GPU Runtime、CPU Runtime 和模型能力允许拆成签名的离线安装包，避免把全部模型塞入一个安装包。

### 2.2 技术路线

最终路线固定为：

```text
Qt 6.9.2 / C++20 桌面程序
    + 独立 C++ 训练进程
    + LibTorch CUDA 训练
    + 自有可导出 ModelGraph
    + ONNX opset 12
    + TensorRT 10.0.1.6 GPU 推理
    + OpenVINO 2026.2 CPU 推理
```

不采用：

- Python GUI。
- Python 训练子进程。
- Nuitka/PyInstaller 训练环境。
- C# 产品主体。
- 在 Qt UI 进程内直接加载 LibTorch、CUDA、TensorRT 或 OpenVINO。
- 依赖任意 Python `.pt`、`.pth`、`.pdparams` 在客户环境直接加载。

### 2.3 项目类型

顶层项目类型固定为：

```text
detection
classification
instance_segmentation
semantic_segmentation
anomaly_detection
line_detection
ocr_detection
ocr_recognition
ocr_pipeline
```

补充规则：

- `classification` 创建时必须固定 `single_label` 或 `multi_label`。
- `anomaly_detection` 同时支持图像级 OK/NG 和像素级异常 Mask。
- `line_detection` 的每个对象是一条独立线段，无类别、无端点方向；端点顺序不影响对象身份。
- OCR 检测、识别、检测识别流水线是三个独立项目类型。
- `YOLO11`、`YOLO26`、`RF-DETR` 是模型家族，不是项目类型。
- `MLSD` 是 `line_detection` 的模型家族。

### 2.4 首版输入范围

- 首版以静态图片为正式输入。
- 图片导入默认复制到项目目录，源文件不被修改。
- 视频导入、抽帧、连续标注不是首版阻断项，接口可预留但不得做占位实现。
- 首版训练项目根目录优先要求本机磁盘；网络盘必须经过专项可靠性和性能验证后再声明支持。
- Windows 路径通过 `QString`、Qt 文件 API 或宽字符 Win32 API 处理，必须支持中文路径。

## 3. 当前机器事实与开始实施前的阻断项

截至 2026-07-21 已核实：

| 项目 | 当前状态 |
|---|---|
| `F:\VisionAIFlowV1` | 尚不存在 |
| Qt | 已安装 `F:\Qt6.9.2\6.9.2\msvc2022_64` |
| CMake | 已安装 `F:\Qt6.9.2\Tools\CMake_64\bin\cmake.exe` |
| VS2022 Build Tools | 已安装 `F:\VS2022\BuildTools` |
| 当前 MSVC | 只有 `14.44.35207` / 19.44 |
| 需要的 MSVC | 需并行安装 `14.36` / 19.36 |
| CUDA Toolkit | 已安装并验证 11.8.89 |
| CUDA 11.2 | 同机可能存在，不得被构建脚本误选 |
| `F:\VisionAIFlowDeps` | 尚不存在 |

在编译 CUDA 源码前必须安装 MSVC v143 14.36。CUDA 11.8 官方编译器支持矩阵覆盖 VS2022 的 19.3x 编译器，但不覆盖当前 19.44。不得把 `--allow-unsupported-compiler` 作为正式方案。

Phase 0 必须验证：

```text
cl.exe version = 19.36.x
nvcc version = 11.8.89
Qt version = 6.9.2
CMake version = 3.30.x
architecture = x64
```

任一项不匹配时，配置阶段必须 `FATAL_ERROR`，不能继续使用偶然找到的其他版本。

## 4. 第三方库冻结矩阵

### 4.1 正式基线

| 分类 | 组件 | 冻结版本 | 使用方式 |
|---|---|---:|---|
| GUI | Qt | 6.9.2 MSVC2022 x64 | 动态链接，Qt Widgets |
| 语言 | C++ | C++20 | 禁止依赖编译器扩展完成核心逻辑 |
| 构建 | CMake | 3.30.5 | CMake Presets |
| 编译器 | MSVC v143 | 14.36 / 19.36 x64 | 整个解决方案统一使用 |
| GPU工具链 | CUDA Toolkit | 11.8.89 | 显式指定路径 |
| 训练 | LibTorch | 2.7.1 + cu118 | Windows shared-with-deps，Release/Debug 分包 |
| GPU推理 | TensorRT | 10.0.1.6 | 与已上线客户端完全一致 |
| 模型交换 | ONNX C++ | 1.15.0 | 编译 ONNX protobuf 和 checker |
| ONNX语义 | opset | 12 | GPU部署强制目标 |
| ONNX IR | IR version | 9 | 模型清单中记录 |
| 序列化 | Protobuf | 3.20.3 | 与 ONNX 一起离线构建 |
| CPU推理 | OpenVINO | 2026.2 | C++ Runtime，CPU 后端 |
| 图像处理 | OpenCV | 4.12.0 | C++，不使用 Python bindings |
| JSON | nlohmann/json | 3.12.0 | 纯 C++ 模块；Qt 边界可使用 QJson |
| 日志 | spdlog | 1.15.3 | 使用内置 fmt，滚动日志 |
| 纯C++测试 | GoogleTest | 1.17.0 | 模型、几何、训练、导出测试 |
| Qt测试 | Qt Test | 6.9.2 | QObject、IPC、UI 测试 |

### 4.2 不单独引入的依赖

- 不引入 `torchvision` C++ 库；所需网络层、损失、数据增强和后处理在本仓库实现并测试。
- 不引入 ONNX Runtime 作为正式推理后端。
- 不引入 Paddle Runtime 作为 OCR 隐式后端。
- 不引入 Ultralytics Python 包。
- 不引入 Anomalib Python 包；按明确算法分别实现，例如 PatchCore、PaDiM、STFPM、EfficientAD。
- 不把 OpenCV DNN 当作 TensorRT/OpenVINO 失败时的静默回退。

### 4.3 版本兼容结论

TensorRT 10.0.1.6 官方支持 CUDA 11.8，并测试过 ONNX 1.15.0。ONNX 库版本和模型 opset 是两个不同维度：ONNX 1.15.0 用于构造和校验模型，生成的部署模型固定为 opset 12。

LibTorch 2.7.1 cu118、TensorRT 10.0.1.6 和 CUDA 11.8 可以共存，但不得让 LibTorch 与 TensorRT 同时加载到同一个进程。进程隔离用于降低 CUDA、cuDNN、CRT 和 DLL 依赖冲突风险。

### 4.4 许可证门禁

正式交付前必须为代码库和模型权重分别登记：

- 名称、版本、官方来源和 SHA-256。
- 代码许可证和权重许可证。
- 商业分发权、修改权和署名要求。
- 是否要求提供源代码或重新链接能力。
- 法务审核结论。

重点门禁：

- Qt 使用商业许可证，或严格满足 LGPL 动态链接和告知义务。
- 如果复用 Ultralytics 代码、训练逻辑或受限权重，必须解决相应商业许可；“最终导出 ONNX”不自动消除上游许可义务。
- RF-DETR 的开放规格和受限规格必须分别登记，不得混用许可证结论。
- 本仓库最终许可证由所有者确认；Codex 不得从旧原型的 AGPL 声明自动推断新产品许可证。

## 5. 总体进程架构

### 5.1 进程划分

```text
VisionAIFlow.exe
  Qt Widgets UI
  项目、数据集、标注、任务、模型包管理
  进程监督、IPC客户端、GPU资源协调
       |
       +-- VisionTrainerHost.exe
       |     LibTorch 2.7.1 cu118
       |     CUDA 11.8
       |     数据加载、训练、验证、checkpoint、ONNX导出
       |
       +-- VisionTensorRtHost.exe
       |     TensorRT 10.0.1.6
       |     CUDA 11.8
       |     ONNX解析、engine构建与GPU推理
       |
       +-- VisionOpenVinoHost.exe
             OpenVINO 2026.2
             CPU推理
```

辅助程序：

```text
VisionAIFlowCli.exe
  项目校验、训练、验证、导出、模型包检查、doctor、自动化验收
```

约束：

- UI 进程不链接 Torch、CUDA、TensorRT 或 OpenVINO。
- `VisionTrainerHost.exe` 不链接 TensorRT 或 OpenVINO。
- `VisionTensorRtHost.exe` 不链接 LibTorch 或 OpenVINO。
- `VisionOpenVinoHost.exe` 不链接 LibTorch、CUDA 或 TensorRT。
- UI 只用 `QProcess` 启动和监督进程，不用标准输出承担实时控制协议。
- 标准输出和标准错误只用于启动前诊断与兜底日志。

### 5.2 IPC

正式 IPC 使用：

- `QLocalServer` / `QLocalSocket`，Windows 下对应本地命名管道。
- 4 字节 little-endian 消息长度 + CBOR payload。
- 控制消息最大 16 MiB，超过上限立即断开并报告协议错误。
- 大图、Mask、特征预览使用 `QSharedMemory`，控制消息只传描述符、尺寸、步长、像素格式、字节数和校验值。
- 每个请求包含 `protocolVersion`、`requestId`、`jobId`、`type`、`timestampUtc`。
- 每个失败响应包含 `errorCode`、`errorMessage`、`recoverable` 和可选 `details`。
- 训练进度建议每 100 至 250 ms 合并发送一次；心跳每 1 s 一次。
- IPC 必须处理分包、粘包、半包、超长包、未知消息、重复 requestId、连接中断和版本不匹配。
- 本地服务名包含随机会话令牌，并限制为当前用户访问。

### 5.3 任务状态机

```text
created
  -> validating
  -> queued
  -> starting
  -> running
  -> pausing -> paused -> running
  -> cancelling -> cancelled
  -> completing -> completed
  -> failed
```

规则：

- 只有 worker 返回 `completed`、制品校验通过并持久化成功，UI 才能标记完成。
- 进程正常退出但没有完成事件属于失败。
- 收到完成事件但进程异常退出属于失败。
- 空检测可以是合法结果；Runtime 异常不能伪装成空检测。
- 取消首先发送结构化 `cancel` 请求，超过配置时间后再 terminate，最后才 kill，并明确记录强制终止。
- UI 关闭时必须提示仍在运行的任务，不得无声杀掉训练。
- 同一张 GPU 默认训练与 TensorRT 推理互斥，通过跨进程 `GpuLease` 管理；不允许 OOM 后自动改为 CPU 并宣称成功。

## 6. 代码分层与依赖方向

### 6.1 主要层次

```text
foundation
  无业务语义的 Result、Error、日志、文件、哈希、时间、版本

domain
  Project、Dataset、Annotation、TrainingJob、ModelArtifact 等领域对象

application
  用例编排、命令、查询、状态机，不依赖具体 UI 或 AI Runtime

project_store / dataset / annotation
  本地文件项目、数据导入、标注读写、几何和格式转换

ipc
  协议、帧、客户端、服务端、共享内存和进程监督

tensor / model_graph / training
  张量设备、模型图、优化器、AMP、数据加载、checkpoint、指标

models
  各模型适配器和模型定义

export
  ONNX opset 12 生成、检查、制品封装

inference
  统一推理接口、预处理、解码、TensorRT、OpenVINO

app / hosts / cli
  可执行程序组合层
```

### 6.2 必须遵守的依赖规则

- `foundation` 不依赖任何业务模块。
- `domain` 只依赖 `foundation`。
- `application` 依赖抽象接口，不依赖 TensorRT、OpenVINO 或具体 Qt Widget。
- `training`、`model_graph`、`models` 不依赖 Qt Widgets。
- `app` 不直接依赖 LibTorch、TensorRT 或 OpenVINO。
- TensorRT 和 OpenVINO 后端共享同一套预处理、坐标还原和后处理合同。
- 模型代码不能直接写项目 JSON；通过 application/domain 接口传递。
- 不允许循环依赖。

### 6.3 错误合同

公共接口使用统一 `Result<T>` 或等价的显式错误类型：

```cpp
struct Error
{
    ErrorCode code;
    std::string message;
    std::map<std::string, std::string> context;
};
```

要求：

- 失败结果的 message 不得为空。
- 顶层进程边界捕获 `std::exception`、Torch、CUDA、TensorRT、OpenVINO 和未知异常，转换成明确错误并记录堆栈可用信息。
- 不允许 `catch (...) {}` 静默吞错。
- 日志中必须包含时间、进程、线程、jobId、requestId、模块和错误码。
- 日志采用滚动文件，防止客户磁盘被无限占用。

## 7. 模型适配器与可导出 ModelGraph

### 7.1 模型适配器接口

首版模型适配器静态链接进 `VisionTrainerHost.exe`，避免通过不稳定的 LibTorch C++ ABI 加载任意 DLL。

每个适配器必须提供等价能力：

```text
id
version
capabilities
supportedProjectTypes
validateDataset
validateConfig
createModelGraph
createLoss
createMetrics
createDecoder
train
resume
evaluate
exportModel
validateArtifact
```

未来新增 SOTA 模型有两条路径：

1. 在同一仓库新增静态适配器并发布新产品版本。
2. 通过独立签名的扩展 Host 进程实现同一 IPC 协议；UI 不加载扩展 DLL，从进程边界保持 ABI 隔离。

首版不实现任意第三方 DLL 模型插件 ABI。

### 7.2 ModelGraph 原则

不能依赖不存在的稳定 LibTorch C++ ONNX 导出器。必须实现自有 `ModelGraph`：

- 一份图定义同时服务 LibTorch forward 和 ONNX 导出。
- 节点具有稳定 ID、输入、输出、属性、形状推导和参数映射。
- 参数注册表确保训练参数名、checkpoint 参数名和 ONNX initializer 名一一对应。
- 训练专用 Loss、数据增强和指标不进入推理图。
- 导出器直接使用 ONNX 1.15 protobuf 构造 ModelProto。
- 导出后执行 ONNX checker、形状检查、算子白名单检查和后端可加载性测试。

首批节点至少包括：

```text
Conv2d
BatchNorm
Linear
Activation
MaxPool
AveragePool
AdaptiveAveragePool
Resize
Concat
Split
Slice
Reshape
Flatten
Transpose
Squeeze
Unsqueeze
ReduceMean
Softmax
Elementwise
MatMul
Gather
Pad
```

SiLU 使用 `x * Sigmoid(x)` 导出。LayerNorm 和 GroupNorm 在 opset 12 下必须由经过数值测试的基础算子分解，不能声称使用 opset 17/18 的函数算子。

### 7.3 opset 12 导出合同

- `ai.onnx` opset 必须等于 12。
- IR version 固定为 9。
- 默认导出原始网络输出，NMS、轮廓、Mask 还原和坐标映射在共享 C++ decoder 中完成。
- 禁止导出来源版本高于 opset 12 的标准算子。
- 禁止未在模型清单声明的 custom op。
- `GridSample` 标准算子从 opset 16 才存在，opset 12 图中不得直接出现。
- 模型导出必须记录输入名称、输出名称、dtype、layout、动态维度和 shape profile。
- 工业首版默认 batch=1；动态尺寸只允许使用显式 min/opt/max profile。
- 任何一次导出都必须生成确定的 SHA-256。

### 7.4 checkpoint 与初始权重

- 训练 checkpoint 使用 LibTorch archive 保存模型、优化器、调度器、AMP scaler、epoch、step、采样器和 CPU/CUDA RNG 状态。
- checkpoint 外层必须有 JSON manifest、完整性哈希和精确产品/适配器/LibTorch版本。
- 恢复训练必须检查模型图签名和参数形状，不允许部分加载后静默继续。
- 客户端不直接加载任意 `.pt`、`.pth` 或 `.pdparams`。
- 上游预训练权重必须先由内部受控转换流程生成经过签名的初始模型包；转换工具不属于客户产品运行时。
- 可使用审核过的 ONNX initializer 作为初始权重来源，但必须严格校验参数映射。

### 7.5 AMP

LibTorch C++ AMP 封装在独立 `AmpController` 中，不能把实验性 API 泄漏到各模型：

- 支持明确的 FP32 和 AMP FP16 配置。
- 自有动态 loss scaler 完成 scale、unscale、非有限梯度检测、skip step、growth/backoff。
- 用户请求 AMP 而 AMP 初始化失败时任务必须失败，不得静默回退 FP32。
- 保存和恢复 scaler 状态。
- 对梯度裁剪规定在 unscale 之后执行。
- 使用 FP32 master statistics 处理需要稳定性的归一化和指标。

## 8. GPU与CPU推理合同

### 8.1 TensorRT 10.0.1.6

- C++ 代码只使用 TensorRT 10.0.1.6 已存在的 API。
- CMake 从 `NvInferVersion.h` 校验 major=10、minor=0、patch=1、build=6。
- ONNX parser、builder 和 runtime 必须来自同一 SDK 包。
- 使用 explicit batch 和 named tensor API。
- 首版正式支持 FP32、FP16；INT8 只有在校准数据、缓存、精度回归和客户配置闭环完成后才能启用。
- 不使用 TensorRT 10.10 构建 engine。
- 不把开发机生成的 `.engine` 当作跨机器通用制品。

部署包默认交付 ONNX，在客户机首次使用时由 TensorRT 10.0.1.6 构建并缓存 engine。缓存键至少包含：

```text
modelSha256
onnxSha256
TensorRT 10.0.1.6
CUDA 11.8
GPU UUID
compute capability
precision
min/opt/max shapes
plugin versions and hashes
builder flags
```

engine 缓存放在 `%LOCALAPPDATA%\VisionAIFlowV1\engine-cache`，不写回签名模型包。加载前必须验证 cache manifest；反序列化失败后可以删除该单个缓存并重新构建，但必须记录原因，不得递归清空整个缓存目录。

### 8.2 OpenVINO 2026.2

- CPU Host 直接加载经过验证的 ONNX，或从同一模型包加载专用 OpenVINO artifact。
- 记录实际设备、精度、线程配置和性能 hint。
- CPU 不支持最低指令集时明确失败。
- 不在 OpenVINO 失败后改用 OpenCV DNN 并隐藏真实后端。
- 如 GPU 和 CPU 需要不同图，模型包允许分别携带 `trt1001_opset12` 和 `openvino_cpu` artifact，二者必须共享同一输入输出语义和 decoder 合同。

### 8.3 预处理与后处理

统一实现并测试：

- RGB/BGR、通道顺序、NCHW/NHWC。
- 归一化、mean/std、scale、零点。
- Resize、letterbox、pad、crop。
- 原图到网络输入的仿射变换及其逆变换。
- bbox、polygon、Mask、ROI、轮廓和线段坐标还原。
- Mask 阈值、插值方式、裁剪边界和空 Mask 语义。
- OCR 文本框排序、旋转、字符字典和未知字符处理。

坐标转换不能分散复制在各后端中。

## 9. 模型接入顺序与兼容策略

| 顺序 | 模型/任务 | opset 12策略 | 备注 |
|---:|---|---|---|
| 1 | 单标签分类 | 标准算子 | 用于验证完整训练基础设施 |
| 2 | 多标签分类 | 标准算子 | BCE、阈值和指标独立 |
| 3 | YOLO11 detection | 原始输出，C++解码和NMS | 首个检测闭环 |
| 4 | U-Net semantic segmentation | Resize使用opset 12兼容表达 | 完成Mask和轮廓链路 |
| 5 | YOLO11 instance segmentation | boxes、coefficients、prototype原始输出 | C++组合Mask |
| 6 | MLSD line detection | 热图/偏移原始输出 | 无类别、无方向线段 |
| 7 | anomaly detection | Backbone进图，特征库和距离计算在C++ | 明确实现具体算法 |
| 8 | OCR detection/recognition/pipeline | 受控网络和字典 | 不承诺任意Paddle模型 |
| 9 | YOLO26 | 使用传统输出模式 | 等价于 `end2end=false` |
| 10 | RF-DETR | 专项适配 | 最后实施，风险最高 |

RF-DETR 如果使用 GridSample 或可变形采样，必须二选一：

1. 分解为经过验证的 opset 12 基础算子。
2. 使用 `com.visionaiflow` custom op，并提供与 TensorRT 10.0.1.6、CUDA 11.8 绑定的 `IPluginV3` 插件。

custom plugin 必须在模型清单中记录名称、版本、namespace、TensorRT版本、CUDA版本、输入输出合同和 SHA-256。插件缺失或版本不符时必须拒绝加载。OpenVINO 使用独立标准图或独立扩展，不能假设 TensorRT 插件可以复用。

## 10. 项目文件格式

### 10.1 项目目录

```text
ProjectRoot
├─ project.json
├─ labels.json
├─ project.lock
├─ data
│  ├─ images
│  ├─ annotations
│  ├─ splits
│  ├─ thumbnails
│  └─ index.json
├─ runs
│  └─ <run-uuid>
│     ├─ request.json
│     ├─ status.json
│     ├─ metrics.jsonl
│     ├─ checkpoints
│     ├─ logs
│     └─ artifacts
├─ models
│  └─ <model-uuid>
│     ├─ reference.json
│     └─ evaluations
└─ backups
```

### 10.2 存储规则

- JSON、JSONL 和 Schema 使用 UTF-8。
- `project.json` 只保存稳定元数据，不保存全量图片和全量标注。
- 每张图片使用独立标注 JSON，文件名使用稳定 UUID，不依赖原始文件名作为主键。
- 所有项目内路径使用相对路径和 `/` 分隔符；不得把开发机绝对路径写入可迁移项目。
- 关键 JSON 使用 `QSaveFile` 原子写入，commit 后重新读取、解析、Schema校验和关键字段比对。
- `metrics.jsonl` 逐行追加，每行独立可解析；异常尾行要显式报告和修复，不能吞掉整份指标。
- `project.lock` 记录 PID、进程启动时间、机器、用户和产品版本；旧锁必须核对进程身份，不能仅按 PID 判断。
- 项目 Schema 需要显式版本和单向迁移器。迁移前备份，迁移失败保持原项目不变。
- `projectType` 和创建时的 `classificationMode` 不可原地修改。
- 标签删除、重命名、合并必须先分析受影响标注并提供可回滚事务文件。
- 导入格式与内部格式分离；YOLO、COCO、PaddleOCR、MLSD 等只是导入导出适配器。

### 10.3 标注对象

内部标注至少覆盖：

```text
classification labels
axis-aligned boxes
polygons
instance masks
semantic masks
image-level anomaly label
pixel-level anomaly mask
undirected line segments
OCR quadrilaterals
OCR text and dictionary metadata
```

所有几何对象必须定义坐标系、边界闭开区间、像素中心语义、归一化方式和裁剪规则。

## 11. 模型包格式

```text
<model-package>
├─ package.json
├─ checksums.json
├─ signature.json
├─ labels.json
├─ preprocessing.json
├─ postprocessing.json
├─ artifacts
│  ├─ trt1001_opset12
│  │  └─ model.onnx
│  └─ openvino_cpu
│     └─ model.onnx
├─ plugins
│  └─ win-x64
└─ licenses
```

`package.json` 至少包含：

```text
packageSchemaVersion
packageId
packageVersion
modelFamily
adapterId
adapterVersion
projectType
inputContract
outputContract
decoderId
classes or dictionary
trainingProvenance
artifact list
supportedProductRange
TensorRT/CUDA/OpenVINO requirements
plugin requirements
license metadata
```

签名验证失败、哈希不符、Schema不兼容或出现路径穿越条目时，模型包安装必须失败并回滚。

## 12. 新仓库目录

```text
F:\VisionAIFlowV1
├─ .gitignore
├─ AGENTS.md
├─ CMakeLists.txt
├─ CMakePresets.json
├─ CMakeUserPresets.example.json
├─ README.md
├─ LICENSE
├─ THIRD_PARTY_NOTICES.md
├─ VISIONAIFLOWV1_CPP_LIBTORCH_EXECUTION_SPEC.md
├─ cmake
│  ├─ CompilerPolicy.cmake
│  ├─ DependencyVersions.cmake
│  ├─ InstallRuntime.cmake
│  ├─ Sanitizers.cmake
│  └─ modules
│     ├─ FindTensorRT.cmake
│     └─ VerifyCudaToolchain.cmake
├─ config
│  ├─ dependencies.lock.json
│  └─ product-version.json
├─ schemas
│  ├─ project
│  ├─ annotation
│  ├─ training
│  ├─ ipc
│  └─ model-package
├─ cpp
│  ├─ foundation
│  ├─ qt_foundation
│  ├─ domain
│  ├─ application
│  ├─ project_store
│  ├─ dataset
│  ├─ annotation
│  │  ├─ geometry
│  │  ├─ formats
│  │  └─ workspace
│  ├─ ipc
│  │  ├─ protocol
│  │  ├─ local_socket
│  │  ├─ shared_memory
│  │  └─ process_supervisor
│  ├─ tensor
│  │  ├─ device
│  │  ├─ memory
│  │  ├─ amp
│  │  └─ serialization
│  ├─ model_graph
│  │  ├─ graph
│  │  ├─ nodes
│  │  ├─ shape_inference
│  │  └─ parameter_registry
│  ├─ training
│  │  ├─ trainer
│  │  ├─ dataloader
│  │  ├─ augmentation
│  │  ├─ losses
│  │  ├─ optimizers
│  │  ├─ schedulers
│  │  ├─ metrics
│  │  ├─ checkpoint
│  │  └─ callbacks
│  ├─ models
│  │  ├─ common
│  │  ├─ classification
│  │  ├─ yolo11
│  │  ├─ unet
│  │  ├─ mlsd
│  │  ├─ anomaly
│  │  ├─ ocr
│  │  ├─ yolo26
│  │  └─ rfdetr
│  ├─ export
│  │  ├─ onnx
│  │  ├─ validators
│  │  └─ model_package
│  ├─ inference
│  │  ├─ api
│  │  ├─ preprocessing
│  │  ├─ postprocessing
│  │  ├─ tensorrt
│  │  └─ openvino
│  ├─ trainer_host
│  ├─ tensorrt_host
│  ├─ openvino_host
│  ├─ cli
│  ├─ app
│  │  ├─ forms
│  │  ├─ viewmodels
│  │  ├─ resources
│  │  └─ main.cpp
│  └─ tests
│     ├─ unit
│     ├─ integration
│     ├─ parity
│     └─ fixtures
├─ docs
│  ├─ architecture
│  ├─ project-format
│  ├─ ipc
│  ├─ model-adapters
│  ├─ build
│  ├─ packaging
│  └─ verification
├─ tools
│  ├─ deps
│  ├─ build
│  ├─ package
│  ├─ schema
│  └─ model-validation
├─ packaging
│  ├─ base
│  ├─ gpu-runtime
│  ├─ cpu-runtime
│  └─ model-packages
├─ third_party
│  ├─ manifests
│  ├─ licenses
│  └─ patches
└─ out
   ├─ build
   ├─ install
   ├─ package
   ├─ test-results
   └─ logs
```

每个 C++ 库统一采用：

```text
<module>/include/visionaiflow/<module>/...
<module>/src/...
<module>/CMakeLists.txt
```

`.cpp`、`.h` 按项目规则保存为 GBK/System；`.ui` 保持 Qt Designer XML；CMake、JSON、Schema、Markdown 使用 UTF-8。所有新增窗体必须有对应 `.ui` 文件，基础控件和简单布局放入 `.ui`。

## 13. Git与依赖仓库管理

### 13.1 Git

- 新目录使用单一 monorepo，C++、Schema、测试、打包和文档保持同一提交兼容。
- 初期使用 `main`、短期功能分支和发布 Tag，不使用复杂 Git Flow。
- 产品发布 Tag 使用 `vMAJOR.MINOR.PATCH`。
- 不使用 Git Submodule 管理核心第三方依赖。
- 不提交模型权重、客户数据、训练结果、engine cache、SDK二进制和解压后的 LibTorch/TensorRT/OpenVINO。
- 不提交 `CMakeUserPresets.json` 中的本机绝对路径。
- `out/`、IDE缓存和运行日志必须进入 `.gitignore`。
- 每个提交必须保持至少一个受支持 Release preset 可配置；阶段结束前必须编译和测试。

### 13.2 外部依赖

大型依赖放在：

```text
F:\VisionAIFlowDeps
├─ libtorch\2.7.1-cu118\release
├─ libtorch\2.7.1-cu118\debug
├─ tensorrt\10.0.1.6-cuda11.8
├─ openvino\2026.2
├─ opencv\4.12.0
├─ onnx\1.15.0
├─ protobuf\3.20.3
├─ nlohmann_json\3.12.0
├─ spdlog\1.15.3
├─ googletest\1.17.0
├─ src
├─ build
├─ install
└─ manifests
```

仓库中的 `config/dependencies.lock.json` 记录版本、文件名、SHA-256、许可证和期望目录。`tools/deps/verify-deps.ps1` 必须完成真实文件、头文件版本宏、DLL/Lib存在性和哈希检查。

正常构建必须完全离线：

```text
FETCHCONTENT_FULLY_DISCONNECTED=ON
```

依赖缺失时给出精确缺失路径、期望版本和修复方式，不允许从网络临时下载，也不允许从系统 PATH 猜测替代版本。

## 14. CMake目标与Preset

### 14.1 主要目标

建议目标名：

```text
vaf_foundation
vaf_qt_foundation
vaf_domain
vaf_application
vaf_project_store
vaf_dataset
vaf_annotation
vaf_ipc
vaf_tensor
vaf_model_graph
vaf_training
vaf_models_common
vaf_model_<name>
vaf_export_onnx
vaf_model_package
vaf_inference_common
vaf_inference_tensorrt
vaf_inference_openvino
VisionAIFlow
VisionTrainerHost
VisionTensorRtHost
VisionOpenVinoHost
VisionAIFlowCli
```

开启：

```text
CMAKE_CXX_STANDARD=20
CMAKE_CXX_STANDARD_REQUIRED=ON
CMAKE_CXX_EXTENSIONS=OFF
CMAKE_AUTOMOC=ON
CMAKE_AUTOUIC=ON
CMAKE_AUTORCC=ON
```

MSVC 至少启用 `/W4`，并将本仓库警告视为错误；第三方头文件使用 SYSTEM include，不能通过全局关闭警告掩盖本仓库问题。

### 14.2 Preset

保留唯一 Qt Kit 名 `QT6_MSVC2022`，提供：

```text
QT6_MSVC2022-Debug
QT6_MSVC2022-Release
QT6_MSVC2022-RelWithDebInfo
```

生成器使用 `Visual Studio 17 2022`、architecture `x64`、toolset `v143,version=14.36`。Release 是客户交付和完整 AI 验收的权威配置，RelWithDebInfo 用于性能诊断。

Preset 必须显式传入：

```text
CMAKE_PREFIX_PATH=F:/Qt6.9.2/6.9.2/msvc2022_64
VISIONAIFLOW_DEPS_ROOT=F:/VisionAIFlowDeps
CUDAToolkit_ROOT=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.8
Torch_DIR=<对应配置>/share/cmake/Torch
OpenVINO_DIR=<OpenVINO runtime cmake目录>
OpenCV_DIR=<OpenCV config目录>
```

TensorRT 使用自有 `FindTensorRT.cmake`，不依赖 SDK 不一定提供的 Config package。

## 15. 从空目录开始的完整构建步骤

以下步骤是 Codex 最终必须实现并验证的正式构建路径。

### 15.1 安装前提

1. 通过 Visual Studio Installer 为 `F:\VS2022\BuildTools` 并行安装 MSVC v143 14.36 x64/x86 build tools 和匹配的 Windows 10/11 SDK。
2. 保留 Qt 6.9.2 MSVC2022 x64。
3. 保留 CUDA Toolkit 11.8.89。
4. 创建 `F:\VisionAIFlowDeps`，按锁文件放置官方依赖包。
5. 每个下载归档先验证 SHA-256，再解压；哈希进入依赖 lock。
6. 正式打包机不得使用来源不明的预编译库。

### 15.2 初始化编译环境

在 `cmd.exe` 中执行：

```bat
call "F:\VS2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 -vcvars_ver=14.36
set VISIONAIFLOW_DEPS_ROOT=F:\VisionAIFlowDeps
set CUDAToolkit_ROOT=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.8
set PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.8\bin;%PATH%
cd /d F:\VisionAIFlowV1
```

验证：

```bat
where cl
cl
where nvcc
nvcc --version
"F:\Qt6.9.2\Tools\CMake_64\bin\cmake.exe" --version
```

输出不满足冻结版本时立即停止。

### 15.3 构建小型源码依赖

Codex 必须实现 `tools\deps\build-source-deps.ps1`，使用 MSVC 14.36、x64 和 `/MD`/`/MDd` 分配置构建 Protobuf 3.20.3、ONNX 1.15.0、spdlog 1.15.3 和 GoogleTest 1.17.0，并安装到 `F:\VisionAIFlowDeps\install\<configuration>`。

脚本要求：

- `-Configuration Debug|Release` 必填。
- 每个依赖使用独立 build 目录。
- Protobuf 禁用 tests，ONNX 禁用 tests，spdlog 使用 bundled fmt。
- ONNX 必须链接本次构建的 Protobuf，不得误用系统其他版本。
- 失败立即退出非零码并输出依赖名、命令和日志路径。
- 完成后运行 `verify-deps.ps1`。
- 不联网、不自动修改系统 PATH、不覆盖其他版本。

正式命令：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\deps\build-source-deps.ps1 -Configuration Release -DepsRoot F:\VisionAIFlowDeps
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\deps\verify-deps.ps1 -Configuration Release -DepsRoot F:\VisionAIFlowDeps
```

### 15.4 配置

```bat
"F:\Qt6.9.2\Tools\CMake_64\bin\cmake.exe" --preset QT6_MSVC2022-Release
```

配置阶段必须输出并写入 `out/build/.../resolved-dependencies.json`：

```text
compiler path and version
Qt version
CUDA path and version
LibTorch version and CUDA tag
TensorRT four-part version
ONNX version and target opset
Protobuf version
OpenVINO version
OpenCV version
Git commit
```

### 15.5 编译

```bat
"F:\Qt6.9.2\Tools\CMake_64\bin\cmake.exe" --build --preset QT6_MSVC2022-Release --parallel
```

构建输出必须只进入 `out/build`，不得把 DLL、生成头文件和中间文件散落在源码目录。

### 15.6 测试

```bat
"F:\Qt6.9.2\Tools\CMake_64\bin\ctest.exe" --preset QT6_MSVC2022-Release --output-on-failure
```

GPU、CPU和模型专项测试通过 CTest label 区分：

```bat
"F:\Qt6.9.2\Tools\CMake_64\bin\ctest.exe" --preset QT6_MSVC2022-Release -L unit --output-on-failure
"F:\Qt6.9.2\Tools\CMake_64\bin\ctest.exe" --preset QT6_MSVC2022-Release -L integration --output-on-failure
"F:\Qt6.9.2\Tools\CMake_64\bin\ctest.exe" --preset QT6_MSVC2022-Release -L cuda --output-on-failure
"F:\Qt6.9.2\Tools\CMake_64\bin\ctest.exe" --preset QT6_MSVC2022-Release -L parity --output-on-failure
```

缺少 GPU 时，GPU测试必须被明确标记为未满足环境并使发布验收失败；不能用空测试冒充通过。开发者可以单独运行不需要 GPU 的 label，但不得因此宣称完整验收通过。

### 15.7 安装到 staging

```bat
"F:\Qt6.9.2\Tools\CMake_64\bin\cmake.exe" --install out\build\QT6_MSVC2022-Release --config Release --prefix out\install\Release
```

随后由正式脚本收集依赖：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\package\stage.ps1 -Configuration Release -DepsRoot F:\VisionAIFlowDeps
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\package\audit-stage.ps1 -StageRoot .\out\install\Release
```

`stage.ps1` 必须调用 Qt 6.9.2 对应的 `windeployqt`，并按组件精确复制所需 DLL；不能复制整个 SDK。`audit-stage.ps1` 必须检查缺失 DLL、错误架构、意外 `.py`、调试 DLL、重复库、未登记许可证和超出清单的文件。

### 15.8 打包

基础包至少拆分为：

```text
VisionAIFlowV1-Base-win-x64
VisionAIFlowV1-Training-cu118-win-x64
VisionAIFlowV1-TensorRT-10.0.1.6-win-x64
VisionAIFlowV1-OpenVINO-2026.2-win-x64
VisionAIFlowV1-Model-<adapter>-<version>-win-x64
```

在安装器方案最终冻结前，必须先生成可复现的 ZIP staging 包和文件清单。安装器不能改变内部组件边界和验收要求。

## 16. 分阶段实施计划

### Phase 0：建立新仓库和治理基线

目标：从空目录建立无 Python 的新仓库。

实施：

- 创建 `F:\VisionAIFlowV1` 并初始化 Git main。
- 创建简洁 `AGENTS.md`，写明 Qt6.9.2、MSVC14.36、编码、UI和错误处理规则。
- 创建根 CMake、Presets、README、许可证占位决策说明、第三方清单和目录骨架。
- 创建依赖 lock 与工具链检查。
- 明确旧项目只读。

验收：

- 仓库无 `python/` 和自研 `.py`。
- 配置时能准确识别缺少 MSVC14.36或依赖，并以明确消息失败。
- Markdown、JSON和CMake为UTF-8；没有把未来 `.cpp/.h` 默认转成UTF-8的脚本。

### Phase 1：CMake、依赖与最小多进程骨架

目标：五个可执行程序完成真实启动、版本报告和 IPC 握手。

实施：

- 完成依赖检测和 imported targets。
- 建立 foundation、domain、ipc 和各 Host。
- 实现 QLocalSocket CBOR framing、心跳、请求响应和错误合同。
- 实现 process supervisor 和结构化日志。

验收：

- Release 全量编译。
- UI 可启动每个 Host，验证版本后正常关闭。
- 协议版本不匹配、半包、超长包、Host崩溃均返回明确错误。
- UI 进程模块列表中没有 Torch、CUDA、TensorRT、OpenVINO DLL。

### Phase 2：文件项目、数据集与项目创建

目标：完成所有项目类型的创建、打开、校验、锁和图片导入。

实施：

- 实现完整 Schema 和迁移框架。
- 实现 `ProjectStore`、`DatasetIndex`、原子写入和项目锁。
- 创建项目 UI 必须用 `.ui`，一次选择项目类型和必要 taskOptions。
- 实现复制导入、重复检测、哈希和失败回滚。

验收：

- 九种项目类型均可创建并重新打开。
- 分类模式、项目类型创建后不可修改。
- 中文路径、同名图片、损坏图片、只读目录、磁盘空间不足有明确结果。
- 写入中断不会留下被当作有效项目的半文件。

### Phase 3：标注工作台

目标：完成各任务共享画布和类型化标注工具。

实施：

- 画布缩放、平移、选择、撤销重做、快捷键和自动保存。
- bbox、polygon、mask、line、OCR quadrilateral、分类和异常标注。
- 标签管理及删除/重命名/合并影响分析。
- 坐标和Mask转换统一进入 geometry 模块。

验收：

- 每种标注保存、关闭、重开无变化。
- 缩放和平移后坐标误差满足像素合同。
- MLSD线段交换端点后视为同一无向线段。
- 空Mask、越界polygon、自交polygon和无有效轮廓得到明确处理结果。
- 撤销重做跨保存点行为有测试。

### Phase 4：训练基础设施和 ModelGraph

目标：建立真实可训练、可恢复、可导出的 C++ 训练内核。

实施：

- CUDA设备检查、显存统计、GpuLease。
- DataLoader、线程池、pinned memory、随机种子、增强。
- 优化器、调度器、AMP、checkpoint、指标和回调。
- ModelGraph节点、形状推导、参数注册和 ONNX exporter。
- ONNX checker 和 opset12白名单。

验收：

- 小型网络在 CPU/CUDA forward/backward 正确。
- checkpoint 恢复后下一步参数更新与未中断基线在容差内一致。
- AMP 检测 overflow 并正确跳过 step，scaler 可恢复。
- ONNX 图通过 checker，且不存在未声明的高版本算子。
- 训练取消、Host崩溃、CUDA OOM、NaN loss 均明确失败。

### Phase 5：分类纵向闭环

目标：完成第一个从项目、训练、导出到双后端推理的完整闭环。

实施：

- 单标签和多标签模型、Loss、指标、数据划分。
- 训练配置 UI、实时曲线、checkpoint、resume、evaluate。
- 导出 opset12 ONNX。
- TensorRT 10.0.1.6 和 OpenVINO 推理。

验收：

- 可在小数据集过拟合，Loss和指标符合预期。
- 训练中断后恢复达到连续训练等价结果。
- LibTorch FP32、TensorRT FP32和OpenVINO输出通过 parity 阈值。
- FP16阈值独立记录，不能套用FP32结果。
- 模型包签名、安装、推理和卸载闭环通过。

### Phase 6：YOLO11 detection

目标：首个正式目标检测闭环。

实施：

- 自有 YOLO11 网络定义、assigner、loss、增强、指标。
- 原始 head 输出、C++ decode 和 NMS。
- bbox坐标还原和可视化。

验收：

- 人工可计算 fixture 验证 assigner/loss/decode/NMS。
- 训练、resume、evaluate、export、TensorRT/OpenVINO闭环。
- 三后端解码前张量和最终 bbox 均通过 parity。
- 空检测与 Runtime失败可区分。

### Phase 7：分割

目标：U-Net语义分割和YOLO11实例分割。

实施：

- U-Net、语义Loss和mIoU/Dice。
- 实例分割prototype、coefficients和Mask组合。
- Mask、轮廓、ROI、缩放和边界还原。

验收：

- 像素级 fixture 覆盖 resize、pad、crop和反变换。
- 空Mask和无有效轮廓是明确业务结果或明确错误，不能混淆。
- 三后端Mask parity和指标回归通过。

### Phase 8：MLSD线段检测

目标：完成无类别、无方向的独立线段项目。

验收：

- 端点交换不改变匹配、指标或序列化语义。
- 重复线段、极短线段、越界线段和零长度线段有确定规则。
- 训练、导出、双后端推理和坐标还原通过。

### Phase 9：异常检测

目标：按算法逐个实现，不使用模糊的“Anomalib兼容”声明。

建议顺序：PatchCore、PaDiM、STFPM、EfficientAD。

验收：

- 每个算法独立声明训练/fit语义、特征库格式和阈值策略。
- 图像级分数、像素热图、Mask和阈值 parity 通过。
- 空embedding、特征库损坏和维度不符明确失败。

### Phase 10：OCR

目标：实现受控的检测、识别和pipeline，不承诺任意Paddle权重兼容。

实施：

- 选择明确的PP-OCR检测和识别网络规格进行C++复现。
- 字典版本化、动态宽度profile、CTC/SVTR类解码按实际模型实现。
- 检测框还原、裁剪、旋转、排序和识别流水线。

验收：

- 中文、英文、数字、长文本、旋转文本和未知字符fixture通过。
- TensorRT/OpenVINO与LibTorch输出和最终文本通过专项阈值。
- 不可转换模型必须报告不支持，不得静默启用Paddle Runtime。

### Phase 11：YOLO26

目标：在现有部署端保持 opset12 兼容。

实施：

- 使用传统 one-to-many 输出语义，等价于 `end2end=false`。
- 不把 end-to-end 后处理固化进 ONNX。
- 复用共享 C++ NMS 和坐标还原。

验收：

- TensorRT 10.0.1.6可构建。
- 输出合同与现有上线端兼容。
- 禁止因为模型默认end-to-end而改变客户端输出语义。

### Phase 12：RF-DETR

目标：在不破坏 opset12 基线的前提下接入高风险模型。

实施顺序：

1. 冻结一个具体RF-DETR规格和许可证。
2. 列出全部ModelGraph缺失算子。
3. 先完成LibTorch训练和checkpoint。
4. 再选择基础算子分解或IPluginV3。
5. 最后完成OpenVINO独立图和parity。

验收：

- 不允许把未支持算子替换成近似空实现。
- plugin ABI、版本、哈希和加载错误完整。
- Windows 10 + RTX20/30代表硬件完成构建与推理。
- 动态batch不是首版要求；batch=1必须稳定。

### Phase 13：迁移、打包和客户验收

目标：完成旧项目导入、模块化离线包和干净机验收。

实施：

- 旧项目只读导入器，不修改原数据。
- 基础、训练、GPU、CPU和模型包分离。
- 第三方许可证、签名、哈希、升级、回滚和卸载。
- Windows 10/11干净机离线安装。

验收：

- 安装目录无自研 `.py`。
- 无VS、Qt、CUDA Toolkit开发环境的客户机可按产品合同运行；只要求合格驱动和随包Runtime。
- 缺失DLL、错误GPU、错误驱动、损坏模型包均明确失败。
- 卸载不删除客户项目和训练结果。
- 升级失败可回滚到上一可用版本。

## 17. 测试与验收标准

### 17.1 通用门禁

每个阶段必须满足：

- Release配置成功。
- 本阶段新增目标全部编译。
- 单元测试、集成测试和适用的GPU/parity测试通过。
- 没有禁用测试、注释错误或降低阈值来换取通过。
- 没有新增编译警告。
- 所有新错误路径包含非空 `errorMessage` 和日志。
- 文档、Schema和实现同步。

### 17.2 数值一致性

每个模型适配器提供 `parity.json`，至少记录：

```text
reference backend
test dataset hash
input preprocessing hash
output names and shapes
FP32 absolute/relative tolerances
FP16 absolute/relative tolerances
task-level metric tolerances
hardware and runtime versions
```

默认起点，不是可以无条件套用的最终阈值：

- FP32中间张量：`atol <= 1e-4`、`rtol <= 1e-4`。
- FP16中间张量：按层和模型批准，默认从 `atol <= 1e-2`、`rtol <= 1e-2` 开始。
- 检测最终框同时比较类别、分数、匹配IoU和数量。
- 分割同时比较logits、Mask像素、Dice/mIoU和轮廓。
- OCR同时比较logits/概率和最终字符串。
- 异常检测同时比较图像分数、热图、Mask和阈值后结果。

阈值必须由真实对比数据确定；放宽阈值需要记录原因和回归证据。

### 17.3 训练正确性

- 每个模型有可快速过拟合的小型确定性数据集。
- 固定随机种子的重复训练在规定容差内可复现。
- resume和连续训练在同一step比较参数、优化器和调度器状态。
- NaN/Inf loss、非有限梯度、CUDA OOM和数据损坏明确失败。
- DataLoader不会重复丢样或跨split泄漏。
- Train/Val/Test支持自动比例和手动划分，划分结果落盘且可重现。

### 17.4 IPC和生命周期

- UI在250ms量级看到稳定进度更新，不依赖控制台文本解析。
- Host心跳丢失、崩溃、卡死、取消和强制终止都有状态测试。
- 共享内存句柄在正常、异常和取消路径均释放。
- 连续启动/停止100次无句柄持续增长。
- 长时间训练日志和指标不会阻塞UI线程。

### 17.5 性能与资源

- 在固定基准机记录吞吐、延迟、CPU、GPU和峰值显存，不凭主观判断宣称提升。
- warm-up后重复推理不得出现持续性内存增长。
- 性能回归默认门禁为相同硬件、相同模型和相同配置下不劣于批准基线10%；超过必须分析和批准。
- TensorRT engine构建和首次加载时间单独记录，不混入稳态推理延迟。

### 17.6 UI验收

- 所有新窗体有 `.ui` 文件。
- UI显示中文，专业术语和OK/NG等可保留英文。
- 训练、导出、推理和模型安装不会阻塞UI线程。
- 错误对话框提供用户可理解摘要和日志定位信息。
- 高DPI、125%/150%缩放和最小支持分辨率完成检查。
- 关键流程有QtTest或自动化集成测试；无法自动化的视觉项有验收清单和截图证据。

### 17.7 客户交付验收

至少覆盖：

- Windows 10 x64 + RTX 20代表机。
- Windows 11 x64 + RTX 30/40代表机。
- AVX2 CPU无NVIDIA GPU环境。
- 完全断网。
- 标准用户权限。
- 中文用户名和中文项目路径。
- 不安装Python、Visual Studio、Qt SDK和CUDA Toolkit开发组件。

必须验证：安装、启动、创建项目、导入、标注、训练、恢复、导出、GPU推理、CPU推理、升级、回滚、卸载和项目保留。

## 18. 安全与可靠性要求

- 模型包和项目归档解压时防止 `..`、绝对路径、符号链接和路径穿越。
- 对JSON/CBOR大小、数组数量、字符串长度、图片尺寸和共享内存大小设上限。
- DLL仅从安装目录和批准的插件目录加载，使用安全DLL搜索策略；不得从项目目录加载任意DLL。
- 模型插件必须签名和哈希校验后加载。
- 不执行项目JSON中提供的命令行。
- 日志不记录客户图片内容、授权密钥或不必要的个人路径信息。
- 文件替换使用原子写，安装和升级使用可回滚事务。
- 任何批量删除必须遵守用户确认规则；实现缓存清理时也必须限制到解析并验证过的专用缓存根目录。

## 19. Codex执行规则

新任务收到本文档后：

1. 确认当前工作目录是 `F:\VisionAIFlowV1`，不是 `F:\VisionAIFlow`。
2. 完整读取 `AGENTS.md` 和本文档。
3. 只读检查本机工具链和依赖，不凭记忆宣称已安装。
4. 用中文建立阶段计划，一次只允许一个 `in_progress` 项。
5. 从 Phase 0 顺序实施；已经有真实代码时先审计再继续，不重复覆盖用户改动。
6. 每一阶段完成后运行对应构建和测试，报告真实命令和结果。
7. 遇到依赖或许可证阻断时明确标记 `blocked/pending`，不得写兼容假实现。
8. 不修改三个旧参考目录。
9. 不批量删除目录；如未来需要清理旧原型或构建目录，先列出精确路径并等待用户确认。
10. 不因为上下文不足重启项目；以Git、代码、测试和本文档继续。

## 20. 明确不属于首版的事项

- 多GPU、多机训练。
- Linux/macOS客户端。
- 视频连续标注。
- 云训练和联网模型下载。
- 任意Python训练脚本兼容。
- 任意第三方LibTorch DLL插件直接加载。
- 任意 `.pt`/`.pdparams` 客户端导入。
- 未完成校准与精度验收的INT8。
- 在未验证时宣称支持所有PaddleOCR、Anomalib或RF-DETR变体。

这些事项只能在已有首版闭环通过后立项，不得以空菜单、占位接口或假成功结果提前出现。

## 21. 实施完成定义

整个重构只有同时满足以下条件才可称为完成：

- 九种项目类型的文件项目和核心标注闭环可用。
- 规定模型按阶段完成真实训练、恢复、验证和导出。
- GPU模型可由TensorRT 10.0.1.6从opset12 ONNX构建并推理。
- CPU模型可由OpenVINO加载并推理。
- LibTorch、TensorRT和OpenVINO数值一致性通过适配器阈值。
- UI与worker交互实时、可取消、可诊断，不解析控制台文本判断成功。
- Windows 10/11离线干净机验收通过。
- 客户安装目录无自研 `.py`，依赖和许可证清单完整。
- 所有失败路径有真实错误信息和日志，无假实现、空实现和静默吞错。
- 文档、AGENTS、Schema、构建脚本、测试和代码保持一致。

## 22. 官方参考

- CUDA 11.8 Windows安装与编译器支持：<https://docs.nvidia.com/cuda/archive/11.8.0/cuda-installation-guide-microsoft-windows/>
- PyTorch历史版本：<https://pytorch.org/get-started/previous-versions/>
- TensorRT 10.0.1 Release Notes：<https://docs.nvidia.com/deeplearning/tensorrt/latest/getting-started/release-notes-10/10.0.1.html>
- TensorRT Engine兼容：<https://docs.nvidia.com/deeplearning/tensorrt/latest/inference-library/engine-compatibility.html>
- ONNX IR：<https://onnx.ai/onnx/repo-docs/IR.html>
- ONNX GridSample：<https://onnx.ai/onnx/operators/onnx__GridSample.html>
- OpenVINO ONNX模型加载：<https://docs.openvino.ai/2026/openvino-workflow/model-preparation/convert-model-onnx.html>
- Ultralytics YOLO导出合同参考：<https://docs.ultralytics.com/modes/export>

本文档中的版本是项目冻结版本，不应仅因为出现更新版本而自动升级。任何升级必须单独完成工具链、ABI、ONNX、TensorRT engine、数值一致性、安装包和客户环境回归后再修改冻结矩阵。
