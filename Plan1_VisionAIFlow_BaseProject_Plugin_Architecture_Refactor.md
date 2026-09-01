# Codex Plan 1：VisionAIFlowV1 BaseProject 架构瘦身与训练插件接口重构

## 0. 任务目标

重构仓库：

```text
F:\VisionAIFlowV1
```

重点处理：

```text
BaseProject/
ModelPlugins/
```

当前 `BaseProject` 存在明显的算法内部过度抽象倾向，例如：

```text
AiEngine/
├─ Export
├─ ModelGraph
├─ Tensor
├─ Training
└─ TrainingState

BaseModel/
├─ Models_Api
└─ Models_Common
```

本次重构目标不是取消“父类 + DLL 插件”架构，而是明确抽象边界：

> 保留“任务级插件接口”，删除或弱化“深度学习框架级抽象”。

最终要求：

- 所有目标检测训练 DLL 统一继承同一个 `IDetectionTrainer`。
- 所有分类训练 DLL 统一继承同一个 `IClassificationTrainer`。
- 所有分割训练 DLL 统一继承同一个 `ISegmentationTrainer`。
- UI 通过统一父接口和下拉框完成算法发现、切换、启动、停止。
- 模型内部直接使用 LibTorch / OpenCV / CUDA 等，不要求经过自定义 Tensor、ModelGraph、Loss、Optimizer、DataLoader 抽象层。
- 以当前已有 `Yolo11.dll` 作为第一阶段验证对象。
- 本计划不移植 YOLOv8。
- 新接口完成并由 YOLO11 验证后冻结，供后续 YOLOv8 / RF-DETR 等直接接入。

---

# 1. 先审计现有依赖，不直接删除代码

Codex 首先扫描：

```text
BaseProject/
ModelPlugins/
UIProject/
VisionAIFlowApp/
SoftwareProject/
TestDemo/
```

重点建立依赖表：

```text
Foundation
Models_Common
Models_Api
TrainingState
Training
Tensor
ModelGraph
Export
Runtime
Data
```

输出：

```text
旧模块
→ 谁引用
→ 暴露什么公共 API
→ 是否属于平台能力
→ 是否属于算法内部实现
→ 保留 / 合并 / 删除 / 延后处理
```

禁止第一步直接大规模删除目录。

---

# 2. 明确新的架构边界

重构后的原则：

```text
UI / App
   │
   ▼
PluginManager
   │
   ▼
任务级公共接口
   │
   ├─ IDetectionTrainer
   ├─ IClassificationTrainer
   └─ ISegmentationTrainer
   │
   ▼
算法 DLL
   │
   ├─ YOLO11.dll
   ├─ YOLOv8.dll（后续 Plan 2）
   ├─ RFDetr.dll（未来）
   ├─ ResNet.dll（未来）
   └─ UNet.dll（未来）
        │
        ▼
LibTorch / OpenCV / CUDA
```

BaseProject 不再承担：

```text
自定义 Tensor 框架
统一 ModelGraph
统一 Layer
统一 Loss
统一 Optimizer
统一网络结构
强制统一 DataLoader 内部实现
```

这些由每个算法插件自己决定。

---

# 3. 新建/整理最小 Plugin API

建议将新的公共插件接口集中到类似：

```text
BaseProject/
└─ PluginApi/
   ├─ Common/
   ├─ Detection/
   ├─ Classification/
   └─ Segmentation/
```

如果当前仓库已有合适公共 API 目录，可复用，不强制必须按此命名。

## Common

至少包含：

```text
ITrainPlugin.h
PluginInfo.h
TrainState.h
TrainProgress.h
PluginCapabilities.h
PluginParameter.h
```

基础接口保持薄：

```cpp
class ITrainPlugin
{
public:
    virtual ~ITrainPlugin() = default;

    virtual PluginInfo pluginInfo() const = 0;
    virtual TrainState state() const = 0;

    virtual bool stop() = 0;
    virtual bool waitForStopped(int timeoutMs) = 0;
};
```

不要在 `ITrainPlugin` 中加入：

```text
forward
backward
loss
optimizer
tensor
layer
graph
dataloader
```

---

# 4. 建立 Detection 父接口

核心新增/重构：

```text
IDetectionTrainer.h
DetectionTrainConfig.h
DetectionTrainProgress.h
DetectionMetrics.h
DetectionTrainerCapabilities.h
```

建议能力：

```cpp
class IDetectionTrainer : public ITrainPlugin
{
public:
    virtual ~IDetectionTrainer() = default;

    virtual bool initialize(const DetectionTrainConfig& config) = 0;
    virtual bool startTrain() = 0;

    virtual DetectionTrainProgress progress() const = 0;

    virtual DetectionTrainerCapabilities capabilities() const = 0;

    virtual bool exportModel(
        const QString& outputPath,
        const QString& format) = 0;
};
```

公共配置只放真正通用项，例如：

```text
datasetPath
outputPath
epochs
batchSize
imageWidth
imageHeight
learningRate
gpuId
numWorkers
useFP16
resume
pretrained
```

不要把 YOLO / DETR 专属字段全部塞入公共结构体。

---

# 5. 算法专属参数机制

Detection 插件必须允许存在算法专属参数。

建议：

```cpp
QVariantMap algorithmOptions;
```

或建立轻量：

```cpp
struct PluginParameterDefinition
{
    QString key;
    QString displayName;
    QVariant defaultValue;
    QVariant minValue;
    QVariant maxValue;
    QString category;
};
```

插件提供：

```cpp
virtual QVector<PluginParameterDefinition>
    parameterDefinitions() const = 0;
```

例如：

YOLO：

```text
close_mosaic
box_gain
cls_gain
dfl_gain
mosaic
mixup
```

RF-DETR：

```text
num_queries
decoder_layers
matcher_cost_class
matcher_cost_bbox
matcher_cost_giou
```

UI 根据当前插件动态生成“高级参数”。

禁止创建一个包含所有算法字段的巨型 `DetectionTrainConfig`。

---

# 6. 建立 Classification / Segmentation 父接口

同样建立：

```text
IClassificationTrainer
ClassificationTrainConfig
ClassificationTrainProgress

ISegmentationTrainer
SegmentationTrainConfig
SegmentationTrainProgress
```

保持任务级抽象，不对模型内部实现做约束。

当前阶段只需把接口骨架、PluginManager 支持和构建链建立好。

如果现有分类插件 `Linear` 能低成本适配，可作为第二个验证样例；否则不要扩大本计划范围。

---

# 7. DLL Factory ABI

真正实现运行时热插拔。

每个 Detection DLL 统一导出：

```cpp
extern "C"
{

PLUGIN_EXPORT
IDetectionTrainer* createDetectionTrainer();

PLUGIN_EXPORT
void destroyDetectionTrainer(
    IDetectionTrainer* trainer);

}
```

分类：

```text
createClassificationTrainer
destroyClassificationTrainer
```

分割：

```text
createSegmentationTrainer
destroySegmentationTrainer
```

主程序不直接：

```cpp
new Yolo11...
```

也不静态链接每个算法插件 `.lib`。

---

# 8. PluginManager

实现或重构统一插件管理器。

职责仅限：

```text
扫描插件目录
加载 DLL
读取 PluginInfo
识别任务类型
resolve Factory
创建实例
销毁实例
卸载 DLL
错误报告
生命周期管理
```

例如：

```text
models/
├─ detection/
│  ├─ Yolo11.dll
│  └─ ...
├─ classification/
└─ segmentation/
```

UI 获取：

```text
Detection 插件列表
Classification 插件列表
Segmentation 插件列表
```

分别填充对应训练界面的下拉框。

---

# 9. 热切换生命周期

必须保证安全卸载。

切换插件时顺序：

```text
stop()
↓
waitForStopped()
↓
训练线程结束
↓
CUDA / LibTorch 工作完成
↓
释放 tensor / module / optimizer / dataloader
↓
destroyXXXTrainer()
↓
QLibrary::unload()
↓
加载新 DLL
```

禁止训练线程未退出时卸载 DLL。

PluginManager 应统一负责该流程。

---

# 10. Yolo11 作为新架构验证对象

当前：

```text
ModelPlugins/Detection/Yolo11
```

已有：

```text
Yolo11Detector
Yolo11DetectionAdapter
Yolo11DetectionDecoder
```

并且 `.pro` 当前显式依赖：

```text
Foundation
Models_Common
Models_Api
TrainingState
LibTorch
```

本计划要求：

## 10.1 保留算法内部实现

`Yolo11Detector` 内部可以继续直接：

```cpp
torch::nn::Module
torch::Tensor
```

不要把其内部网络、loss、target assign、augmentation 重新抽象。

## 10.2 新增薄 Adapter

新增或重构成类似：

```cpp
class Yolo11TrainPlugin final
    : public IDetectionTrainer
{
};
```

Adapter 只负责：

```text
DetectionTrainConfig
→ 转成 YOLO11 内部参数

startTrain
→ 调用 Yolo11Trainer

stop
→ 设置停止信号

progress
→ 转换内部训练状态

exportModel
→ 调用 YOLO11 内部导出
```

不要把 YOLO11 算法代码搬入 BaseProject。

---

# 11. 逐步减少 Yolo11 对 BaseProject 旧模块依赖

目标是让 `Yolo11.pro` 最终尽量只依赖：

```text
新的 PluginApi
必要 Foundation（若仍有真正通用 Result/Logging）
LibTorch
OpenCV（若需要）
CUDA runtime（若需要）
```

对：

```text
Models_Common
Models_Api
TrainingState
Training
Tensor
ModelGraph
```

逐项判断。

如果某个类型只是：

```text
DetectionBox
TrainState
Result
```

应迁入更轻的公共 API 或保留一个极小 Common 库，而不是为了一个 struct 继续依赖整个旧模块。

---

# 12. BaseProject 清理原则

完成 YOLO11 新接口适配后，再删除确认无引用的旧层。

优先检查：

```text
AiEngine/ModelGraph
AiEngine/Tensor
AiEngine/Training
BaseModel/Models_Api
BaseModel/Models_Common
```

分类：

## 保留

真正平台级能力，例如：

```text
日志
Result/Error
任务生命周期
插件描述
插件发现
通用训练状态
公共任务配置 DTO
公共指标 DTO
```

## 删除/合并

只为包装 LibTorch 而存在的层，例如：

```text
Tensor wrapper
ModelGraph wrapper
Layer abstraction
Optimizer abstraction
Loss abstraction
算法训练框架 abstraction
```

## 暂不删除

仍被其他模块真实使用且重构成本过大的代码。

允许留下 deprecated 目录，但必须明确：

```text
禁止新插件继续依赖
后续删除
```

---

# 13. 构建系统

遵守当前仓库正式技术路线：

```text
Qt 6.9.2
C++20
MSVC
qmake/.pro
LibTorch 2.7.1 cu118
CUDA 11.8
```

不要引入新的 CMake 产品构建入口。

继续复用：

```text
qmake/common.pri
VAF_ROOT
VAF_TORCH_ROOT
现有输出目录变量
```

所有新公共 API 和 PluginManager 必须加入现有 `.pro` 构建链。

---

# 14. UI 验证

目标检测训练 UI 必须能够：

```text
启动程序
↓
扫描 Detection DLL
↓
下拉框显示 YOLO11
↓
选择 YOLO11
↓
显示公共参数
↓
显示 YOLO11 专属高级参数
↓
开始训练
↓
获取 epoch/loss/metric/progress
↓
停止训练
↓
安全释放
```

不得让目标检测 UI include：

```text
Yolo11*.h
```

UI 只能依赖：

```text
IDetectionTrainer
PluginManager
公共 DTO
```

---

# 15. 测试

至少新增以下测试：

## PluginManager

```text
发现 Detection DLL
识别插件类型
加载成功
创建实例
销毁实例
卸载 DLL
不存在 DLL 的错误处理
错误 ABI 的错误处理
```

## Yolo11

```text
通过 IDetectionTrainer 初始化
开始训练最小数据集
进度回调正常
停止正常
释放正常
再次加载正常
```

## 热切换

即使当前只有 Yolo11，也至少测试：

```text
加载 Yolo11
→ 创建
→ 销毁
→ unload
→ 再次 load
→ 再次创建
```

确保生命周期正确。

---

# 16. 接口冻结点

Plan 1 完成后冻结以下接口：

```text
ITrainPlugin
IDetectionTrainer
IClassificationTrainer
ISegmentationTrainer

PluginInfo
PluginCapabilities

DetectionTrainConfig
DetectionTrainProgress
DetectionMetrics

ClassificationTrainConfig
ClassificationTrainProgress

SegmentationTrainConfig
SegmentationTrainProgress

PluginParameterDefinition

Factory ABI
PluginManager 加载协议
```

后续 Plan 2 移植 YOLOv8 时：

> 不允许因为 YOLOv8 实现差异重新设计 BaseProject。

除非发现明确的通用 ABI 缺陷，否则只能修改 YOLOv8 插件自身。

---

# 17. 禁止事项

Codex 禁止：

1. 不要取消任务级父类。
2. 不要取消 DLL 插件架构。
3. 不要让 UI 直接依赖具体算法类。
4. 不要让 UI 静态链接 Yolo11。
5. 不要创建新的 Tensor 包装框架。
6. 不要创建新的 ModelGraph 框架。
7. 不要统一所有算法的 Loss。
8. 不要统一所有算法的 Optimizer。
9. 不要统一所有算法的内部 DataLoader。
10. 不要为了未来 RF-DETR 预先设计大量未使用接口。
11. 不要在本计划移植 YOLOv8。
12. 不要一次性删除仍有真实引用的模块。
13. 不要改变 YOLO11 算法行为，除非为接口适配所必需。
14. 不要改用 CMake 作为正式产品构建入口。

---

# 18. 最终验收标准

- [ ] Detection / Classification / Segmentation 均有明确父接口。
- [ ] `IDetectionTrainer` 是目标检测 UI 唯一算法接口。
- [ ] DLL 通过统一 Factory 创建和销毁。
- [ ] PluginManager 能运行时扫描/加载/卸载 Detection DLL。
- [ ] Yolo11 已适配新 `IDetectionTrainer`。
- [ ] UI 不 include Yolo11 具体头文件。
- [ ] Yolo11 内部继续直接使用 LibTorch。
- [ ] 不要求 Yolo11 使用自定义 Tensor/ModelGraph/Loss/Optimizer 框架。
- [ ] Yolo11 可通过统一接口开始/停止训练。
- [ ] Yolo11 可安全 unload/reload。
- [ ] 公共接口支持插件能力查询。
- [ ] 公共接口支持算法专属高级参数。
- [ ] 旧 BaseProject 中确认无用的过度抽象模块已删除或标记 deprecated。
- [ ] 新插件不得再依赖 deprecated 算法抽象层。
- [ ] qmake 根工程 Release/Debug 构建通过。
- [ ] 新接口已冻结，可进入 YOLOv8 移植阶段。

---

# Codex 核心原则

> BaseProject 只负责“平台层抽象”：插件 ABI、任务类别、生命周期、配置、进度、日志、能力、Factory 和 PluginManager。

> 模型 DLL 负责“算法内部实现”：网络、Tensor、Loss、Optimizer、DataLoader、Augmentation、Checkpoint 细节，直接使用 LibTorch/OpenCV/CUDA。

> 同类别算法共享父接口，但绝不要求不同算法共享内部训练实现。

> Plan 1 完成并通过 YOLO11 验证后冻结接口，再执行 YOLOv8 移植。
