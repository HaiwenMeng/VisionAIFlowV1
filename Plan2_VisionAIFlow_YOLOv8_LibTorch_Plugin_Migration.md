# Codex Plan 2：koba-jon YOLOv8 C++ LibTorch 训练代码移植为 VisionAIFlowV1 Detection 插件

## 0. 前置条件

本计划必须在：

```text
Codex Plan 1：BaseProject 架构瘦身与训练插件接口重构
```

完成并验收后执行。

必须已经存在并冻结：

```text
IDetectionTrainer
DetectionTrainConfig
DetectionTrainProgress
DetectionMetrics
DetectionTrainerCapabilities
PluginParameterDefinition
Factory ABI
PluginManager
```

本计划原则：

> 只新增 YOLOv8 Detection 插件，不重新设计 BaseProject。

如果 YOLOv8 的内部实现与 YOLO11 不同，应在 YOLOv8 DLL 内解决，而不是重新增加 Tensor / ModelGraph / Loss / Optimizer 等公共抽象。

---

# 1. 源与目标

源：

```text
https://github.com/koba-jon/pytorch_cpp/tree/master/Object_Detection/YOLOv8
```

目标：

```text
F:/VisionAIFlowV1
```

建议目标路径：

```text
ModelPlugins/
└─ Detection/
   ├─ Yolo11/
   └─ YoloV8/
```

并加入：

```text
ModelPlugins/Detection/Detection.pro
```

---

# 2. 移植策略

不要把原项目整体重构成 VisionAIFlow 内部框架。

采用：

```text
原 YOLOv8 算法源码
        │
        ▼
YoloV8Trainer
        │
        ▼
薄 YoloV8TrainPlugin Adapter
        │
        ▼
IDetectionTrainer
```

即：

```text
VisionAIFlow UI
     │
PluginManager
     │
IDetectionTrainer
     │
YoloV8TrainPlugin.dll
     │
YoloV8Trainer
     │
原始 YOLOv8 LibTorch 实现
```

---

# 3. 保留的原 YOLOv8 实现

优先保留原仓库中：

```text
networks.*
loss.*
detector.*
augmentation.*
```

以及 YOLOv8 训练真正依赖的通用：

```text
datasets
dataloader
transforms
utils
```

Codex 先分析实际 include 关系，只复制必要文件。

禁止无差别复制整个 `pytorch_cpp` 仓库。

---

# 4. train.cpp 改造为 YoloV8Trainer

原项目训练入口偏：

```text
main.cpp
→ 命令行参数
→ train(...)
```

迁移后不要保留 CLI 作为正式入口。

改造成：

```cpp
class YoloV8Trainer
{
public:
    bool initialize(const YoloV8TrainConfig& config);
    bool train();
    void requestStop();

    YoloV8TrainProgress progress() const;

private:
    ...
};
```

原训练循环：

```cpp
for epoch
    for batch
        forward
        loss
        backward
        optimizer.step
```

原则上保持算法行为，只替换：

```text
命令行参数
shell
Linux 路径
stdout-only 日志
```

为：

```text
C++ config
插件 progress
插件 log callback
停止信号
Windows 路径
```

---

# 5. 建立 YoloV8TrainPlugin

新增：

```cpp
class YoloV8TrainPlugin final
    : public IDetectionTrainer
{
public:
    PluginInfo pluginInfo() const override;
    DetectionTrainerCapabilities capabilities() const override;

    bool initialize(
        const DetectionTrainConfig& config) override;

    bool startTrain() override;

    bool stop() override;

    bool waitForStopped(
        int timeoutMs) override;

    DetectionTrainProgress
        progress() const override;

    bool exportModel(
        const QString& path,
        const QString& format) override;
};
```

Adapter 只负责：

```text
平台 DTO
↔
YOLOv8 内部配置/状态
```

不要在 Adapter 实现 YOLOv8 网络、loss、NMS 或 augmentation。

---

# 6. YOLOv8 内部配置

建立插件专属：

```cpp
struct YoloV8TrainConfig
{
    ...
};
```

由：

```text
DetectionTrainConfig
+
algorithmOptions
```

转换得到。

公共项使用平台配置：

```text
datasetPath
outputPath
epochs
batchSize
imageSize
learningRate
gpuId
numWorkers
useFP16
resume
pretrained
```

YOLOv8 专属参数放入：

```text
algorithmOptions
```

例如原项目已有/涉及：

```text
model variant
augmentation
prob threshold
nms threshold
mosaic
mixup
flip
scale
blur
brightness
hue
saturation
shift
crop
```

不要把这些反向加进 BaseProject 的通用 `DetectionTrainConfig`。

---

# 7. YOLOv8 Variant

插件至少支持原项目实际实现的模型 variant。

Codex 从源码确认是否支持：

```text
yolov8n
yolov8s
yolov8m
yolov8l
yolov8x
```

如果原实现并非全部支持，以源码实际实现为准。

通过：

```text
parameterDefinitions()
```

提供：

```text
model_variant
```

下拉参数。

不要复制 YOLO11 variant 逻辑来硬套。

---

# 8. 数据集适配

原 YOLOv8 示例目录结构为类似：

```text
trainI/
trainO/
validI/
validO/
testI/
testO/
```

VisionAIFlow 实际训练数据入口应优先使用项目现有检测数据规范。

Codex 先检查当前 VisionAIFlow Detection UI / Data 模块 / YOLO11 使用的数据描述方式。

优先实现一个薄 Dataset Adapter：

```text
VisionAIFlow Detection Dataset
       ↓
YoloV8Dataset
       ↓
原 YOLOv8 dataloader
```

不要为了兼容原仓库强制用户创建 Linux 风格 symlink 或原始固定目录。

Windows 下直接使用真实路径。

---

# 9. 标注格式

原项目训练标签是 YOLO 风格：

```text
class
x_center
y_center
width
height
```

均归一化。

如果 VisionAIFlow 内部检测数据不是直接 YOLO txt：

- 在 Dataset Adapter 层完成转换；
- 优先内存转换；
- 必要时生成临时训练缓存；
- 不修改公共 Detection 插件 API。

不要把数据格式转换逻辑塞进 UI。

---

# 10. Windows / Qt / qmake 移植

当前 VisionAIFlow 正式路线：

```text
Qt 6.9.2
C++20
MSVC
qmake/.pro
LibTorch 2.7.1 cu118
CUDA 11.8
```

原项目：

```text
CMake
Linux
shell scripts
Boost program_options
OpenMP
```

迁移要求：

## 删除正式运行依赖

```text
train.sh
test.sh
detect.sh
demo.sh
Linux symlink
Boost program_options CLI
```

## 改用

```text
qmake .pro
QString / std::filesystem
VisionAIFlow TrainConfig
PluginManager
```

---

# 11. YoloV8.pro

新增：

```text
ModelPlugins/Detection/YoloV8/YoloV8.pro
```

使用：

```qmake
VAF_ROOT = $$clean_path($$PWD/../../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += dll

QT += core

TARGET = YoloV8
DESTDIR = $$VAF_MODELS_DIR
```

LibTorch 依赖复用：

```text
VAF_TORCH_ROOT
```

不要硬编码本机绝对路径。

根据实际使用链接：

```text
torch
torch_cpu
torch_cuda
c10
c10_cuda
```

具体 lib 名称以当前仓库和 LibTorch 2.7.1 实际配置为准，不猜测。

---

# 12. OpenCV

原 YOLOv8 使用 OpenCV。

优先复用 VisionAIFlow 当前统一 OpenCV 配置。

不要：

```text
重新引入另一套 OpenCV 版本
硬编码系统 OpenCV 路径
```

如果仓库已有 `.pri`，直接复用。

---

# 13. OpenMP

原 `pytorch_cpp` 项目要求 OpenMP，并主要用于 CPU 数据处理/并行加载。

Windows/MSVC 下按仓库编译器能力启用。

优先：

```text
/openmp
```

或当前 MSVC 支持的 OpenMP 编译配置。

不要假定 OpenMP 会加速 LibTorch GPU kernel。

重点用于：

```text
CPU 数据预处理
数据加载
augmentation
```

并限制与训练线程/DataLoader线程产生的过度线程竞争。

若原实现依赖 GNU 专用 OpenMP 行为，改成 MSVC 兼容实现。

---

# 14. LibTorch CUDA

插件内部直接使用：

```cpp
torch::Device
torch::Tensor
torch::nn
torch::optim
```

GPU 选择：

```cpp
torch::Device(
    torch::kCUDA,
    gpuId);
```

不得增加：

```text
VafTensor
TensorBridge
ModelGraphTensor
```

等新的中间包装层。

---

# 15. 训练线程

`startTrain()` 不应阻塞 Qt UI 主线程。

采用现有 VisionAIFlow 线程机制或标准：

```text
std::thread
QThread
```

但训练算法类本身尽量不继承 `QThread`。

推荐：

```text
YoloV8TrainPlugin
   │
   ├─ worker thread
   │
   └─ YoloV8Trainer
```

`stop()` 设置：

```cpp
std::atomic_bool stopRequested;
```

训练循环在安全点检查：

```text
batch 边界
epoch 边界
validation 边界
```

不得强制 terminate 线程。

---

# 16. Progress / Log

把原来的：

```text
std::cout
progress bar
console log
```

转换到平台回调。

至少上报：

```text
currentEpoch
totalEpochs
currentBatch
totalBatches

totalLoss
boxLoss
clsLoss
dflLoss（若实现有）

learningRate

precision
recall
mAP50
mAP50_95（若当前实现支持）

elapsed
state
```

如果原仓库没有某项 metric，不伪造。

---

# 17. Capability

YoloV8 插件必须真实声明：

```text
supportFP16
supportResume
supportPretrained
supportOnnxExport
supportMultiGPU
```

只有实际完成并测试的功能才能标 true。

不要为了 UI 一致性返回虚假 capability。

---

# 18. Checkpoint

优先保持 YOLOv8 原生 LibTorch checkpoint 逻辑。

至少保存：

```text
model state
optimizer state（若 LibTorch API/当前实现允许）
epoch
best metric
训练配置
class list
```

通过平台统一：

```text
resume=true
checkpointPath
```

传入。

内部 checkpoint 文件格式无需与 YOLO11 完全一致。

平台只要求生命周期和结果语义一致。

---

# 19. 模型导出

本计划最低要求：

```text
训练产生可再次加载的 LibTorch checkpoint
```

ONNX 导出如果当前 LibTorch C++ 实现稳定可实现，则接入：

```text
exportModel(..., "onnx")
```

若无法稳定从纯 C++ LibTorch 导出 YOLOv8 ONNX：

- 不要伪造；
- capability 标记 false；
- 保留明确 TODO；
- 不因此修改 BaseProject。

TensorRT engine 转换不属于本计划核心范围。

---

# 20. Factory ABI

DLL 必须实现 Plan 1 冻结的 Detection Factory，例如：

```cpp
extern "C"
{

PLUGIN_EXPORT
IDetectionTrainer*
createDetectionTrainer();

PLUGIN_EXPORT
void destroyDetectionTrainer(
    IDetectionTrainer* trainer);

}
```

PluginInfo：

```text
name = YOLOv8
task = Detection
vendor/source = koba-jon/pytorch_cpp
version = 当前移植版本
```

---

# 21. Detection.pro

加入：

```text
YoloV8
```

确保：

```text
Yolo11
YoloV8
```

均生成独立 DLL。

二者之间：

```text
不得互相链接
不得 include 对方头文件
```

共同依赖只能来自冻结后的 PluginApi 和真正通用基础库。

---

# 22. UI 验证

目标检测训练界面启动后：

```text
算法:
[ YOLO11 ▼ ]
```

下拉框自动出现：

```text
YOLO11
YOLOv8
```

切换 YOLOv8 后：

- 公共训练参数保持；
- 高级参数区域切换成 YOLOv8 参数；
- UI 不需要重新编译 YOLOv8 专属逻辑；
- UI 不 include YOLOv8 头文件。

---

# 23. 热切换测试

重点测试：

```text
加载 YOLO11
↓
切到 YOLOv8
↓
加载 YOLOv8
↓
开始训练
↓
停止
↓
销毁 YOLOv8
↓
切回 YOLO11
```

再测试：

```text
YOLOv8
→ stop
→ wait
→ destroy
→ unload
→ reload
→ start
```

确保无：

```text
崩溃
CUDA context 残留导致异常
线程未退出
DLL unload 失败
```

---

# 24. 算法正确性验证

先建立原始 koba-jon YOLOv8 基线。

使用同一：

```text
模型 variant
训练数据
image size
batch size
epoch
optimizer
learning rate
augmentation
seed（能固定则固定）
```

比较：

```text
初始 loss
若干 epoch 后 loss
验证集结果
输出 shape
forward 数值范围
checkpoint 可加载性
```

Windows/LibTorch 版本差异允许存在浮点误差，但不允许：

```text
loss NaN
梯度不更新
输出尺寸错误
label assign 错误
训练完全不收敛
```

---

# 25. 最小 Smoke Test

提供一个极小数据集测试，例如：

```text
2~20 张图片
1~2 类
1~2 epoch
batch 1~2
```

用于 CI/本地快速验证：

```text
插件加载
dataset parse
forward
loss
backward
optimizer step
checkpoint
stop
reload
```

不要求用完整数据集做每次编译验证。

---

# 26. 第三方许可证

保留并检查：

```text
koba-jon/pytorch_cpp
```

的 MIT License。

将必要声明加入：

```text
THIRD_PARTY_NOTICES.md
```

或仓库现有第三方许可体系。

保留源文件必要版权/来源说明。

---

# 27. 禁止事项

Codex 禁止：

1. 不要修改 Plan 1 已冻结的 Detection Plugin API，除非存在明确 ABI bug。
2. 不要新增 Tensor 抽象框架。
3. 不要新增 ModelGraph 框架。
4. 不要新增统一 LossBase。
5. 不要新增统一 OptimizerBase。
6. 不要把 YOLOv8 loss 移入 BaseProject。
7. 不要把 YOLOv8 network 移入 BaseProject。
8. 不要让 YOLOv8 继承 YOLO11 类。
9. 不要让 YoloV8.dll 链接 Yolo11.dll。
10. 不要让 UI include YOLOv8 具体头文件。
11. 不要保留 Boost program_options 作为产品训练入口。
12. 不要保留 shell 脚本作为产品依赖。
13. 不要要求 Linux symlink 数据结构。
14. 不要切换产品构建系统到 CMake。
15. 不要无关重写原 YOLOv8 算法。
16. 不要为了“统一”把 RF-DETR 等未来算法需求提前塞入本插件。
17. 不要伪造未实现的 capability 或 metric。

---

# 28. 最终验收标准

- [ ] `ModelPlugins/Detection/YoloV8` 已建立。
- [ ] `YoloV8.dll` 可独立构建。
- [ ] YoloV8 继承 `IDetectionTrainer`。
- [ ] YoloV8 不依赖 Yolo11。
- [ ] YoloV8 内部直接使用 LibTorch。
- [ ] 原 `networks/loss/detector/augmentation` 主体尽量保持。
- [ ] CLI `train.cpp` 已重构为可调用 Trainer。
- [ ] Windows 路径正常。
- [ ] qmake 构建正常。
- [ ] LibTorch CUDA 训练可运行。
- [ ] OpenCV 正常。
- [ ] OpenMP/MSVC 编译正常或已用等价线程方式兼容。
- [ ] 数据集可以从 VisionAIFlow Detection 数据入口读取。
- [ ] YOLO 标注解析正确。
- [ ] UI 下拉框可同时显示 YOLO11 / YOLOv8。
- [ ] UI 可无算法专属 include 完成切换。
- [ ] YOLOv8 可以 start / stop / wait / destroy / unload。
- [ ] 插件可重新加载。
- [ ] 训练 loss 正常下降。
- [ ] checkpoint 可保存和恢复。
- [ ] capability 与实际功能一致。
- [ ] 第三方 License 已处理。
- [ ] Plan 1 的 BaseProject 架构没有因为本次移植重新膨胀。

---

# Codex 核心原则

> 这是“算法移植 + 薄插件 Adapter”，不是“把 YOLOv8 重写成 VisionAIFlow 自研训练框架”。

> YOLOv8 网络、Tensor、Loss、Optimizer、Augmentation、DataLoader 等内部实现尽量保持原生 LibTorch 结构。

> VisionAIFlow 只通过 `IDetectionTrainer` 看到 YOLOv8。

> 如果 YOLOv8 与 YOLO11 内部结构完全不同，也只能通过统一任务级接口兼容，不能要求二者共享内部算法实现。
