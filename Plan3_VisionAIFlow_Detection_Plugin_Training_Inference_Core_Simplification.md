# VisionAIFlow 检测插件训练推理统一与 Core 简化改动计划

## 1. 计划目标

本计划只处理以下两条直接相关的代码链:

1. 简化 `BaseProject/Core` 的过细构建层级.
2. 将检测模型插件从仅训练接口调整为训练, LibTorch CUDA 推理, 模型导出和 Backbone 导出的统一任务级插件接口.

目标不是建立通用深度学习框架, 也不是提前实现 OpenVINO 或 TensorRT 部署后端.

## 2. 已确定的架构结论

### 2.1 BaseProject 是否过度分层

`BaseProject` 本身不是多余层. 它承担公共库集合和根构建入口职责, 应继续保留:

```text
BaseProject/
├─ Core/
├─ Data/
├─ Runtime/
└─ PluginApi/
```

当前过细的是:

```text
BaseProject/Core/Foundation
BaseProject/Core/Domain
```

二者当前合计只有:

```text
Foundation
├─ Error.h / Error.cpp
└─ Result.h

Domain
├─ JobState.h / JobState.cpp
└─ ProjectType.h / ProjectType.cpp
```

为这些文件分别生成 `Foundation.dll` 和 `Domain.dll` 会增加:

- 两个 qmake 子项目.
- 两套导出宏.
- 两个 DLL/LIB 的构建和复制步骤.
- 所有消费者的重复链接配置.
- Foundation 到 Domain 的人为构建顺序.

因此将二者合并为一个 `VisionAIFlowCore` 动态库.

不使用 `BaseDefine` 名称. 这些代码不仅包含常量和枚举, 还包含 `Result`, `Error`, 字符串转换和状态迁移逻辑. `VisionAIFlowCore` 更符合实际职责.

### 2.2 保留逻辑命名空间

物理构建项目合并后, 暂不合并命名空间:

```cpp
visionaiflow::foundation
visionaiflow::domain
```

头文件路径也继续保持语义清晰:

```text
visionaiflow/foundation/Error.h
visionaiflow/foundation/Result.h
visionaiflow/domain/JobState.h
visionaiflow/domain/ProjectType.h
```

本次只删除过细的 DLL 和 qmake 层级, 不进行无收益的全仓命名空间改名.
给baseproject的所有的项目/子项目的.pro中的SUBDIRS添加注释

### 2.3 检测插件接口

不新增以下接口:

```text
IInferencePlugin
IDetectionInferencer
IInferenceBackend
InferenceBackendFactory
```

当前约束是每个检测模型插件必须同时支持训练和软件内推理. 因此将 `IDetectionTrainer` 改为完整的 `IDetectionPlugin`.

### 2.4 推理后端

当前软件内推理固定使用 LibTorch CUDA:

```text
训练: LibTorch CUDA
软件内验证和单图推理: LibTorch CUDA
部署模型输出: 保留导出接口
OpenVINO/TensorRT 部署运行时: 不在本计划内
```

公共接口不得出现 `torch::Tensor`, `cv::Mat`, OpenVINO 或 TensorRT 类型.

## 3. 目标目录结构

```text
BaseProject/
├─ BaseProject.pro
├─ Core/
│  ├─ Core.pro
│  ├─ include/
│  │  └─ visionaiflow/
│  │     ├─ foundation/
│  │     │  ├─ Error.h
│  │     │  └─ Result.h
│  │     └─ domain/
│  │        ├─ JobState.h
│  │        └─ ProjectType.h
│  └─ src/
│     ├─ Error.cpp
│     ├─ JobState.cpp
│     └─ ProjectType.cpp
├─ Data/
├─ Runtime/
└─ PluginApi/
   ├─ PluginApi.pro
   ├─ include/visionaiflow/plugin_api/
   │  ├─ PluginApi.h
   │  └─ PluginManager.h
   └─ src/PluginManager.cpp

ModelPlugins/
└─ Detection/
   └─ YoloV8/
      ├─ YoloV8.pro
      ├─ yolov8plugin.h
      ├─ yolov8plugin.cpp
      ├─ yolov8plugin.json
      ├─ yolov8trainer.h
      ├─ yolov8trainer.cpp
      ├─ yolov8inference.h
      ├─ yolov8inference.cpp
      └─ third_party/koba_jon/

VisionAIFlowApp/
├─ TrainForms/DetecForms/
│  ├─ detecttrainingcontroller.h
│  └─ detecttrainingcontroller.cpp
├─ InferForms/DetecForms/
│  ├─ detectioninferencecontroller.h
│  └─ detectioninferencecontroller.cpp
└─ valsetform.*
```

`YoloV8Trainer` 和 `YoloV8Inference` 是普通具体类. 不为它们创建内部父接口.

## 4. 公共接口设计

### 4.1 ITrainPlugin

保留现有训练生命周期职责:

```cpp
class ITrainPlugin
{
public:
    virtual ~ITrainPlugin() = default;

    virtual PluginInfo pluginInfo() const = 0;
    virtual TrainState state() const = 0;
    virtual QString errorMessage() const = 0;
    virtual bool stop() = 0;
    virtual bool waitForStopped(int timeoutMs) = 0;
};
```

不向 `ITrainPlugin` 添加检测推理方法. 分类, 检测和分割的推理结果类型不同.

### 4.2 IDetectionPlugin

将 `IDetectionTrainer` 直接替换为 `IDetectionPlugin`:

```cpp
class IDetectionPlugin : public ITrainPlugin
{
public:
    ~IDetectionPlugin() override = default;

    virtual bool initializeTraining(
        const DetectionTrainConfig &config) = 0;

    virtual bool startTrain() = 0;

    virtual DetectionTrainProgress progress() const = 0;

    virtual DetectionMetrics metrics() const = 0;

    virtual bool loadInferenceModel(
        const DetectionInferConfig &config) = 0;

    virtual bool infer(
        const DetectionInferRequest &request,
        DetectionInferResult *result) = 0;

    virtual bool exportModel(
        const ModelExportConfig &config) = 0;

    virtual bool exportBackbone(
        const BackboneExportConfig &config) = 0;

    virtual DetectionPluginCapabilities capabilities() const = 0;

    virtual QVector<PluginParameterDefinition>
    parameterDefinitions() const = 0;
};
```

### 4.3 预训练模型加载

不新增 `loadPretrained()`.

训练预训练权重继续由以下字段传入:

```cpp
DetectionTrainConfig::pretrainedPath
```

`initializeTraining()` 必须完成预训练权重读取和校验. 文件不存在, 权重不匹配或加载异常时返回 `false`, 设置明确 `errorMessage`, 并输出日志.

### 4.4 推理 DTO

新增以下 DTO:

```cpp
struct DetectionInferConfig final
{
    QString modelPath;
    int gpuId{0};
    int imageWidth{0};
    int imageHeight{0};
    bool useFp16{false};
};

struct DetectionInferRequest final
{
    QString imagePath;
    double confidenceThreshold{0.25};
    double nmsThreshold{0.45};
};

struct DetectionBox final
{
    int classId{-1};
    QString className;
    double confidence{0.0};
    double x{0.0};
    double y{0.0};
    double width{0.0};
    double height{0.0};
};

struct DetectionInferResult final
{
    int imageWidth{0};
    int imageHeight{0};
    QVector<DetectionBox> boxes;
};
```

坐标契约固定为原图像素坐标:

```text
x, y: 左上角
width, height: 原图像素尺寸
```

插件内部负责输入缩放, padding 和坐标还原. App 不得了解 YOLO letterbox 或输出张量格式.

### 4.5 导出 DTO

新增:

```cpp
struct ModelExportConfig final
{
    QString checkpointPath;
    QString outputPath;
    QString format;
    int imageWidth{0};
    int imageHeight{0};
};

struct BackboneExportConfig final
{
    QString checkpointPath;
    QString outputDirectory;
    QString format;
    int imageWidth{0};
    int imageHeight{0};
};
```

本计划只允许真实导出. 不允许创建空文件, 假元数据或硬编码成功返回.

如果当前模型暂不具备某类真实导出实现:

- 对应 capability 必须为 `false`.
- UI 不显示或禁用该操作.
- 接口被直接调用时返回 `false` 和明确的 `UnsupportedOperation` 错误.

### 4.6 Capabilities

将 `DetectionTrainerCapabilities` 改名为 `DetectionPluginCapabilities`.

至少包含:

```cpp
struct DetectionPluginCapabilities final
{
    bool supportsResume{false};
    bool supportsPretrained{false};
    bool supportsExport{false};
    bool supportsBackboneExport{false};
    bool supportsFp16{false};
    bool supportsMultiGpu{false};
};
```

推理是检测插件的必备能力, 不增加 `supportsInference`.

### 4.7 Qt 插件 IID

删除旧 IID:

```text
visionaiflow.plugin_api.IDetectionTrainer/1.0
```

使用新 IID:

```cpp
#define VISIONAIFLOW_DETECTION_PLUGIN_IID \
    "visionaiflow.plugin_api.IDetectionPlugin/2.0"
```

不保留旧接口兼容 wrapper. `PluginApi`, 所有检测插件和 App 必须同步重编.

## 5. PluginManager 调整

### 5.1 类型替换

将以下成员:

```cpp
QHash<QString, IDetectionTrainer *> m_loadedTrainers;
IDetectionTrainer *m_detectionTrainer;
```

替换为:

```cpp
QHash<QString, IDetectionPlugin *> m_loadedPlugins;
IDetectionPlugin *m_detectionPlugin;
```

### 5.2 公共方法

保留扫描和加载入口, 将获取方法调整为:

```cpp
bool loadDetectionPlugin(const QString &filePath);
IDetectionPlugin *detectionPlugin() const;
```

删除:

```cpp
IDetectionTrainer *detectionTrainer() const;
```

不新增单独的 `detectionInferencer()`.

### 5.3 插件扫描

`scanDetectionPluginMetadata()` 必须校验:

- IID 为 `IDetectionPlugin/2.0`.
- `plugin_id`, `display_name`, `version`, `task_type` 完整.
- `task_type` 为 `detection`.
- capability 字段与插件实现一致.

失败必须返回包含 DLL 路径的明确错误信息.

### 5.4 卸载和生命周期

插件卸载前:

1. 如果训练状态为 `Running` 或 `Stopping`, 调用 `stop()`.
2. 调用 `waitForStopped(timeoutMs)`.
3. 等待失败则不卸载 DLL, 返回明确错误.
4. 训练线程结束后才允许释放 `QPluginLoader`.

## 6. YoloV8 插件改动

### 6.1 文件重命名

```text
yolov8trainplugin.h    -> yolov8plugin.h
yolov8trainplugin.cpp  -> yolov8plugin.cpp
yolov8trainplugin.json -> yolov8plugin.json
YoloV8TrainPlugin      -> YoloV8Plugin
```

### 6.2 插件入口

```cpp
class YoloV8Plugin final
    : public QObject,
      public plugin_api::IDetectionPlugin
{
};
```

入口类负责:

- Qt 插件元数据.
- 公共接口参数转换.
- 训练线程生命周期.
- 将训练调用转发到 `YoloV8Trainer`.
- 将推理调用转发到 `YoloV8Inference`.
- 保存最新错误信息和训练进度.

入口类不复制网络实现, Loss 或 NMS.

### 6.3 YoloV8Inference

新增一个普通具体类:

```cpp
class YoloV8Inference final
{
public:
    bool loadModel(
        const plugin_api::DetectionInferConfig &config,
        QString *errorMessage);

    bool infer(
        const plugin_api::DetectionInferRequest &request,
        plugin_api::DetectionInferResult *result,
        QString *errorMessage);
};
```

它不是新抽象层, 只用于隔离具体模型推理资源和代码长度.

### 6.4 LibTorch CUDA 推理要求

`loadModel()` 必须完成:

1. 校验 checkpoint 路径.
2. 校验 CUDA 设备存在且 `gpuId` 有效.
3. 构建与 checkpoint 对应的 YOLOv8 模型规格.
4. 加载完整权重并校验参数名称和形状.
5. 将模型移动到目标 CUDA 设备.
6. 切换为 `eval()`.
7. 完成一次真实预热前向.

`infer()` 必须使用:

```cpp
c10::InferenceMode inferenceMode;
```

执行链必须完整:

```text
读取图片
-> 校验图片
-> letterbox/resize
-> BGR/RGB 转换
-> HWC/NCHW 转换
-> 归一化
-> CUDA Tensor
-> forward
-> 输出解码
-> confidence 过滤
-> NMS
-> 映射回原图坐标
-> DetectionInferResult
```

禁止返回空的假结果. 无检测目标可以成功返回空 `boxes`, 但模型失败, 输出张量错误或后处理失败必须返回 `false`.

### 6.5 训练与外部推理互斥

当前不实现训练和外部推理并发.

当状态为:

```text
Running
Stopping
```

调用 `loadInferenceModel()` 或 `infer()` 必须失败并返回明确错误.

不为此增加额外状态机. 直接使用现有 `TrainState` 和实际推理模型对象生命周期判断.

### 6.6 模型规格

一个 YoloV8 插件覆盖:

```text
yolov8n
yolov8s
yolov8m
yolov8l
yolov8x
```

模型规格继续通过 `algorithmOptions["model_variant"]` 传递. 不为每个规格创建 DLL 或派生类.

## 7. VisionAIFlowApp 改动

### 7.1 DetectTrainingController

将所有:

```cpp
detectionTrainer()
initialize(...)
```

替换为:

```cpp
detectionPlugin()
initializeTraining(...)
```

训练控制器继续只负责:

- 从 UI 收集训练配置.
- 加载插件.
- 启动和停止训练.
- 轮询进度.
- 将错误和状态通过 Qt signal 返回 UI.

### 7.2 DetectionInferenceController

新增一个 App 层具体控制器, 不新增公共父接口.

职责:

- 选择已发现的检测插件.
- 调用 `loadInferenceModel()`.
- 单图调用 `infer()`.
- 文件夹模式下遍历受支持图片.
- 将每张图结果保存为同名 txt.
- 将插件错误完整返回 UI.

文件夹推理不得进入 `IDetectionPlugin`.

### 7.3 ValSetForm

将现有推理页面接入 `DetectionInferenceController`.

本次只替换检测任务推理链. 不修改分类, 分割, SAM2 或 SAM3 推理逻辑.

UI 中涉及控件变更时必须在 `valsetform.ui` 中完成布局和文字调整.

### 7.4 txt 输出

文件夹推理输出文件规则:

```text
输入: image001.jpg
输出: image001.txt
```

txt 坐标格式必须由 App 层统一定义. 插件只返回原图像素坐标结果, 不直接写文件.

写入失败必须包含目标路径和系统错误信息.

## 8. Core 合并改动

### 8.1 Core.pro

将 `Core.pro` 从 `subdirs` 项目改成单一 DLL 项目:

```qmake
TEMPLATE = lib
CONFIG += dll
QT += core
TARGET = VisionAIFlowCore
DEFINES += VISIONAIFLOW_CORE_LIBRARY
```

统一编译:

```text
src/Error.cpp
src/JobState.cpp
src/ProjectType.cpp
```

### 8.2 导出宏

新增统一宏:

```cpp
#if defined(VISIONAIFLOW_CORE_LIBRARY)
#define VISIONAIFLOW_CORE_EXPORT __declspec(dllexport)
#else
#define VISIONAIFLOW_CORE_EXPORT __declspec(dllimport)
#endif
```

删除:

```text
VISIONAIFLOW_FOUNDATION_LIBRARY
VISIONAIFLOW_FOUNDATION_EXPORT
VISIONAIFLOW_DOMAIN_LIBRARY
VISIONAIFLOW_DOMAIN_EXPORT
```

### 8.3 删除目录

文件移动并验证完成后删除:

```text
BaseProject/Core/Foundation/Foundation.pro
BaseProject/Core/Domain/Domain.pro
BaseProject/Core/Foundation/
BaseProject/Core/Domain/
```

批量删除执行前必须再次列出绝对路径并确认仅包含已迁移内容.

### 8.4 链接替换

所有有效目标将:

```qmake
-lFoundation
-lDomain
```

替换为:

```qmake
-lVisionAIFlowCore
```

同时将:

```qmake
Foundation.lib
Domain.lib
```

替换为:

```qmake
VisionAIFlowCore.lib
```

直接影响文件至少包括:

```text
BaseProject/Data/ProjectStore/ProjectStore.pro
BaseProject/Data/Annotation/Annotation.pro
BaseProject/Runtime/Ipc/Ipc.pro
BaseProject/Runtime/QtFoundation/QtFoundation.pro
VisionAIFlowApp/VisionAIFlowApp.pro
仍然有效的 Core/Data/Runtime 单元测试 pro
```

### 8.5 include 路径

`qmake/common.pri` 中删除:

```text
BaseProject/Core/Foundation/include
BaseProject/Core/Domain/include
```

替换为:

```text
BaseProject/Core/include
```

`qmake/cuda.pri` 中的 Foundation include 也同步替换.

### 8.6 BaseProject.pro

继续保留:

```text
Core
Data
Runtime
PluginApi
```

`Core.file` 仍指向 `Core/Core.pro`.

`PluginApi` 当前没有引用 Core 类型, 因此删除无实际必要的:

```qmake
PluginApi.depends = Core
```

Data 和 Runtime 继续依赖 Core.

## 9. qmake 依赖收敛

当前 `qmake/common.pri` 会让所有公共库都检查 CUDA, TensorRT, OpenVINO 和 LibTorch. 这会让 `VisionAIFlowCore` 和 `PluginApi` 被无关依赖阻塞.

本次调整为:

```text
qmake/common.pri
└─ Release 配置, Qt 路径, 输出目录, 通用编译选项

qmake/libtorch_cuda.pri
└─ LibTorch CUDA 路径检查, include 和 libs
```

`YoloV8.pro` 显式 include `libtorch_cuda.pri`.

VisionAIFlowApp 现有 SAM/TensorRT/OpenVINO 依赖继续由 `VisionAIFlowApp.pro` 自己声明. 本计划不改动 SAM 推理后端.

不得把 OpenVINO 或 TensorRT 添加到检测插件公共 API.

## 10. 文件级改动清单

### 10.1 新增

```text
BaseProject/Core/include/visionaiflow/foundation/Error.h
BaseProject/Core/include/visionaiflow/foundation/Result.h
BaseProject/Core/include/visionaiflow/domain/JobState.h
BaseProject/Core/include/visionaiflow/domain/ProjectType.h
BaseProject/Core/src/Error.cpp
BaseProject/Core/src/JobState.cpp
BaseProject/Core/src/ProjectType.cpp
qmake/libtorch_cuda.pri
ModelPlugins/Detection/YoloV8/yolov8plugin.h
ModelPlugins/Detection/YoloV8/yolov8plugin.cpp
ModelPlugins/Detection/YoloV8/yolov8plugin.json
ModelPlugins/Detection/YoloV8/yolov8inference.h
ModelPlugins/Detection/YoloV8/yolov8inference.cpp
VisionAIFlowApp/InferForms/DetecForms/detectioninferencecontroller.h
VisionAIFlowApp/InferForms/DetecForms/detectioninferencecontroller.cpp
```

Core 文件是移动后的目标路径, 不重新实现业务逻辑.

### 10.2 修改

```text
BaseProject/BaseProject.pro
BaseProject/Core/Core.pro
BaseProject/PluginApi/PluginApi.pro
BaseProject/PluginApi/include/visionaiflow/plugin_api/PluginApi.h
BaseProject/PluginApi/include/visionaiflow/plugin_api/PluginManager.h
BaseProject/PluginApi/src/PluginManager.cpp
BaseProject/Data/ProjectStore/ProjectStore.pro
BaseProject/Data/Annotation/Annotation.pro
BaseProject/Runtime/Ipc/Ipc.pro
BaseProject/Runtime/QtFoundation/QtFoundation.pro
ModelPlugins/Detection/YoloV8/YoloV8.pro
ModelPlugins/Detection/YoloV8/yolov8trainer.*
VisionAIFlowApp/VisionAIFlowApp.pro
VisionAIFlowApp/TrainForms/DetecForms/detecttrainingcontroller.*
VisionAIFlowApp/valsetform.*
qmake/common.pri
qmake/cuda.pri
```

### 10.3 删除

```text
BaseProject/Core/Foundation/
BaseProject/Core/Domain/
ModelPlugins/Detection/YoloV8/yolov8trainplugin.h
ModelPlugins/Detection/YoloV8/yolov8trainplugin.cpp
ModelPlugins/Detection/YoloV8/yolov8trainplugin.json
```

删除前必须确认新文件已加入对应 `.pro` 并成功编译.

## 11. 实施顺序

必须按以下顺序执行, 每个阶段完成并验证后停止, 等待用户指定下一阶段:

### 阶段 1: 合并 VisionAIFlowCore

1. 将 Foundation 和 Domain 文件移动到 Core.
2. 合并 `Core.pro`.
3. 统一导出宏.
4. 更新 include 和链接路径.
5. 构建 Core, Data, Runtime 和 VisionAIFlowApp Release.
6. 删除旧 Foundation/Domain 目录和旧 DLL/LIB.

验收:

- 只生成 `VisionAIFlowCore.dll/lib`.
- 不再生成或链接 `Foundation` 和 `Domain`.
- 原头文件 include 语义保持不变.

### 阶段 2: 更新 PluginApi

1. 新增推理和导出 DTO.
2. `IDetectionTrainer` 改为 `IDetectionPlugin`.
3. `initialize` 改为 `initializeTraining`.
4. 更新 capability 和 IID.
5. 更新 PluginManager.
6. 构建 PluginApi Release.

验收:

- 仓库有效源码中不再出现 `IDetectionTrainer`.
- 不保留旧 IID 或兼容 wrapper.
- PluginApi 不依赖 LibTorch/OpenCV/CUDA.

### 阶段 3: 更新 YoloV8 插件

1. 重命名插件入口文件和类.
2. 实现新的 `IDetectionPlugin`.
3. 实现真实 LibTorch CUDA 模型加载.
4. 实现单图预处理, forward, 解码, NMS 和坐标还原.
5. 接入真实导出能力或准确声明 capability 为 false.
6. 构建 YoloV8 Release.

验收:

- Qt 插件元数据可以被扫描.
- `qobject_cast<IDetectionPlugin *>` 成功.
- 真实 checkpoint 可以在 CUDA 上完成单图推理.
- 缺失模型和 CUDA 异常均明确失败.

### 阶段 4: 接入 VisionAIFlowApp

1. 更新训练控制器使用 `IDetectionPlugin`.
2. 新增具体推理控制器.
3. 将检测推理页面接入插件.
4. 实现文件夹遍历和 txt 输出.
5. 删除被替换的检测 Python/QProcess 调用链, 不影响其他任务类型.
6. 构建 VisionAIFlowApp Release.

验收:

- App 不包含 YoloV8 具体头文件.
- App 可从 `AIModelPlugins` 发现插件.
- 可加载 checkpoint 并执行单图推理.
- 文件夹模式逐图调用单图接口并生成同名 txt.

### 阶段 5: 直接影响链验证

只验证本次修改影响的链路:

```text
Core -> Data/Runtime -> VisionAIFlowApp
PluginApi -> YoloV8 -> DetectTrainingController
PluginApi -> YoloV8 -> DetectionInferenceController -> ValSetForm
```

不执行全仓全模型回归.

## 12. Release 验证要求

所有构建仅使用:

```text
Qt 6.7.3
MSVC 2019 x64
Release
```

禁止生成或链接 Debug 库.

建议最小构建目标:

```text
VisionAIFlowCore
ProjectStore
Annotation
Ipc
QtFoundation
PluginApi
YoloV8
VisionAIFlowApp
```

C++ 文件修改后必须使用项目根目录 `.clang-format` 格式化本次修改文件.

## 13. 推理正确性验证

使用一个真实 YOLOv8 checkpoint 和固定测试图片完成:

1. checkpoint 加载成功.
2. 模型处于 `eval()`.
3. 使用 `c10::InferenceMode`.
4. CUDA 设备和输入 Tensor 一致.
5. 输出张量数量和形状符合模型规格.
6. 解码结果类别索引不越界.
7. NMS 后坐标位于原图边界内.
8. 相同输入重复推理结果稳定.
9. 空目标图片成功返回空 boxes.
10. 缺失图片, 无效 checkpoint 和 CUDA 异常明确失败.

性能计时必须:

- 完成预热.
- 在 CUDA 计时前后同步.
- 分离预处理, forward, 后处理和总耗时.
- 不把首次模型加载时间计入稳定推理延迟.

## 14. 明确不实施的内容

本计划不包含:

- OpenVINO 推理实现.
- TensorRT 推理实现.
- 推理后端工厂.
- 同时训练和推理.
- 多模型并发加载.
- 网络服务或远程推理.
- 分类和分割插件的推理接口改造.
- 额外依赖安装和版本冻结.
- 兼容旧 `IDetectionTrainer/1.0` 的 wrapper.
- 与本次调用链无关的 UI 或 BaseProject 清理.

## 15. 最终验收标准

- [ ] `Foundation` 和 `Domain` 已合并为单一 `VisionAIFlowCore` 库.
- [ ] BaseProject 仍保持 Core, Data, Runtime, PluginApi 四个清晰模块.
- [ ] `qmake/common.pri` 不再强制所有公共库检查 AI 推理依赖.
- [ ] `IDetectionTrainer` 已替换为 `IDetectionPlugin`.
- [ ] 不存在 `IInferencePlugin` 或 `IDetectionInferencer`.
- [ ] 每个检测模型家族只需要一个插件 DLL.
- [ ] YoloV8 插件真实实现训练和 LibTorch CUDA 单图推理.
- [ ] 训练预训练权重通过 `pretrainedPath` 加载.
- [ ] 文件夹推理和 txt 输出只存在于 App 层.
- [ ] 公共接口不暴露第三方推理框架类型.
- [ ] 所有失败返回明确 `errorMessage` 并输出日志.
- [ ] 受影响目标通过 MSVC 2019 x64 Release 构建.
