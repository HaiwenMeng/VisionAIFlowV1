# TrtSAM2Lib / TrtSAM3Lib 迁移与 SamBaseLib 解耦计划

## 目标

将源仓库：

```text
F:\QtProject\

SamProject/
├─ TrtSam2Lib/
├─ TrtSam3Lib/
└─ SamBaseLib/
```

中的 `TrtSam2Lib`、`TrtSam3Lib` 迁移到：

```text
F:\VisionAIFlowV1

└─ LVMs/
   ├─ TrtSAM2Lib/
   └─ TrtSAM3Lib/
```

迁移后要求：

- `TrtSAM2Lib` 不再继承 `SamBase`。
- `TrtSAM3Lib` 不再继承 `SamBase`。
- 两个库均不再 include、链接或运行时依赖 `SamBaseLib`。
- SAM2 与 SAM3 互相独立，不建立彼此依赖。
- 不重新设计 SAM2/SAM3 推理算法。
- 保持现有 TensorRT 推理行为、模型加载方式和主要公共 API。
- 使用 `VisionAIFlowV1` 当前的 qmake、CUDA、TensorRT、OpenCV 和输出目录规范。

---

## 一、目标目录结构

迁移完成后建议：

```text
VisionAIFlowV1/
└─ LVMs/
   ├─ LVMs.pro
   │
   ├─ TrtSAM2Lib/
   │  ├─ TrtSAM2Lib.pro
   │  ├─ TrtSam2Lib_global.h
   │  ├─ trtsam2lib.h
   │  ├─ trtsam2lib.cpp
   │  └─ third_party/
   │     └─ sam2src/
   │
   └─ TrtSAM3Lib/
      ├─ TrtSAM3Lib.pro
      ├─ TrtSam3Lib_global.h
      ├─ trtsam3lib.h
      ├─ trtsam3lib.cpp
      └─ third_party/
         └─ sam3src/
```

不要把 `SamBaseLib` 一起迁入 `LVMs`。

---

## 二、迁移源码

从原仓库复制：

```text
SamProject/TrtSam2Lib
SamProject/TrtSam3Lib
```

核心需要保留：

```text
trtsam2lib.h
trtsam2lib.cpp
TrtSam2Lib_global.h
third_party/sam2src/**

trtsam3lib.h
trtsam3lib.cpp
TrtSam3Lib_global.h
third_party/sam3src/**
```

第三方 SAM2/SAM3 推理源码原则上保持原样。

仅允许修改：

- include 路径；
- qmake 编译配置；
- SamBaseLib 解耦；
- 类型名称；
- CUDA 编译适配；
- TensorRT/CUDA/OpenCV 依赖路径。

不要借此次迁移重构 TensorRT 推理流程、CUDA kernel、mask 后处理或模型接口。

---

## 三、彻底删除 SamBase 继承

### TrtSAM2Lib

删除：

```cpp
#include "../SamBaseLib/sambaselib.h"
```

将：

```cpp
class TRTSAM2LIB_EXPORT TrtSam2 : public SamBase
```

修改为：

```cpp
class TRTSAM2LIB_EXPORT TrtSam2
```

删除所有成员函数后的：

```cpp
override
```

例如保持为：

```cpp
~TrtSam2();

bool initialize(...);
void setCurrentImage(...);

bool isInitialized() const;
QString modelDir() const;
QString displayName() const;
```

### TrtSAM3Lib

执行相同处理：

```cpp
class TRTSAM3LIB_EXPORT TrtSam3
```

不能再出现：

```text
SamBase
SamBaseLib
sambaselib.h
```

---

## 四、将 SamBaseLib 中的结果结构体收归各自 DLL

当前 `SamBaseLib` 除纯虚接口外，还承担公共结果类型定义。

迁移后不能继续依赖它。

同时不要简单在两个库里各自声明同名：

```cpp
SamObjectResult
SamInferResult
```

否则调用程序同时 include SAM2/SAM3 两个头文件时可能产生全局类型重定义。

### SAM2 类型

在 `trtsam2lib.h` 中定义 SAM2 专属类型：

```cpp
struct TRTSAM2LIB_EXPORT TrtSam2ObjectResult
{
    bool success = false;
    QString errorMessage;
    QString labelName;
    float score = 0.0f;

    QVector<double> roiData;
    QVector<double> minRectRoiData;
    QVector<QPointF> maskContour;
};

struct TRTSAM2LIB_EXPORT TrtSam2InferResult
{
    bool success = false;
    QString errorMessage;
    QVector<TrtSam2ObjectResult> objects;
};
```

然后修改公共接口：

```cpp
TrtSam2InferResult inferByPoint(...);
TrtSam2InferResult inferByRect(...);
TrtSam2InferResult inferByRects(...);
```

并修改 `.cpp` 内：

```text
SamInferResult
SamObjectResult
```

为：

```text
TrtSam2InferResult
TrtSam2ObjectResult
```

### SAM3 类型

同样定义：

```cpp
struct TRTSAM3LIB_EXPORT TrtSam3ObjectResult
{
    bool success = false;
    QString errorMessage;
    QString labelName;
    float score = 0.0f;

    QVector<double> roiData;
    QVector<double> minRectRoiData;
    QVector<QPointF> maskContour;
};

struct TRTSAM3LIB_EXPORT TrtSam3InferResult
{
    bool success = false;
    QString errorMessage;
    QVector<TrtSam3ObjectResult> objects;
};
```

对应接口改成：

```cpp
TrtSam3InferResult inferByPoint(...);
TrtSam3InferResult inferByRect(...);
TrtSam3InferResult inferByRects(...);
```

### 约束

结果结构体字段及含义必须与原 `SamBaseLib` 保持一致。

本次不要新建：

```text
SamCommonLib
SamCommon
SamBase2
ISamBase
```

等新的公共 DLL 或抽象层。

目标是：

> TrtSAM2Lib 和 TrtSAM3Lib 均可以完全独立使用。

---

## 五、尽量保持原公共 API

除结果类型名称变化外，尽可能保持现有 API：

```cpp
initialize()
setCurrentImage()

isInitialized()
modelDir()
displayName()

inferByPoint()
inferByRect()
inferByRects()
```

避免无关 API 修改。

SAM2 原有 encoder / decoder TensorRT engine 机制继续保留。

SAM3 原有 TensorRT engine、Prompt、CUDA 后处理等逻辑继续保留。

---

## 六、重写 TrtSAM2Lib.pro

不要直接复制旧 `.pro`。

使用目标仓库统一环境。

开头参考：

```qmake
VAF_ROOT = $$clean_path($$PWD/../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += dll

QT += core gui

TARGET = TrtSAM2Lib

DEFINES += TRTSAM2LIB_LIBRARY
```

输出目录使用项目已有变量，例如：

```qmake
DESTDIR = $$BUILDLIB

OBJECTS_DIR = $$PWD/release
MOC_DIR = $$PWD/release
RCC_DIR = $$PWD/release
UI_DIR = $$PWD/release
```

需要时将 DLL post-link 到：

```text
BINX64
```

具体写法优先参考 `VisionAIFlowV1` 内已有 Foundation / DLL 工程。

---

## 七、重写 TrtSAM3Lib.pro

基础结构：

```qmake
VAF_ROOT = $$clean_path($$PWD/../..)
include($$VAF_ROOT/qmake/common.pri)

TEMPLATE = lib
CONFIG += dll

QT += core gui

TARGET = TrtSAM3Lib

DEFINES += TRTSAM3LIB_LIBRARY
```

输出目录同样采用：

```qmake
$$BUILDLIB
$$BINX64
```

不要保留源工程绝对路径。

---

## 八、清除旧工程硬编码路径

检查两个 `.pro` 和源码，删除类似：

```text
F:/ytprojectv2alln/BINX64_YoloTraingrayV2
F:/YtProjectV2ALL/...
../SamBaseLib

-lSamBaseLib

$$(CUDA_PATH)
$$(TENSORRT_PATH)
```

TensorRT 使用目标仓库变量：

```qmake
$$VAF_TENSORRT_ROOT
```

CUDA 使用：

```qmake
$$VAF_CUDA_ROOT
```

OpenCV 优先复用：

```text
VisionAIFlowV1/BaseLibX64/V4depends/Opencv
```

或目标工程已有 `.pro/.pri` 中实际使用的 OpenCV 配置。

Codex 应先搜索仓库已有 OpenCV 链接方式并复用，不自行猜测 `.lib` 文件名或版本号。

---

## 九、SAM3 CUDA 编译规则迁移

SAM3 包含 CUDA 文件，例如：

```text
memory.cu
postprocess.cu
preprocess.cu
process_kernel_warp.cu
```

这些文件必须继续由 `nvcc` 编译。

优先参考：

```text
VisionAIFlowV1/qmake/cuda.pri
```

以及仓库已有 CUDA 工程。

CUDA 编译规则必须使用：

```qmake
VAF_CUDA_ROOT
VAF_TENSORRT_ROOT
VAF_ROOT
```

不要继续使用旧：

```text
CUDA_PATH
TENSORRT_PATH
SamBaseLib
```

CUDA include 至少覆盖：

```text
sam3src
CUDA
TensorRT
OpenCV
Qt/项目必要头文件
```

不要修改 CUDA kernel 算法实现，除非迁移后存在明确编译错误且修改仅用于兼容目标环境。

---

## 十、更新 LVMs.pro

当前 `LVMs` 作为 subdirs 工程。

修改为类似：

```qmake
TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += \
    TrtSAM2Lib \
    TrtSAM3Lib

TrtSAM2Lib.file = TrtSAM2Lib/TrtSAM2Lib.pro
TrtSAM3Lib.file = TrtSAM3Lib/TrtSAM3Lib.pro
```

不要添加：

```qmake
TrtSAM3Lib.depends = TrtSAM2Lib
```

或者反向依赖。

两个工程必须平级、完全独立。

---

## 十一、静态依赖检查

迁移完成后，对：

```text
LVMs/TrtSAM2Lib
LVMs/TrtSAM3Lib
```

全局搜索：

```text
SamBase
SamBaseLib
sambaselib
SAMBASELIB
-lSamBaseLib
../SamBaseLib

ytprojectv2alln
YtProjectV2ALL
```

结果必须为 0。

特别确认：

```cpp
class TrtSam2
class TrtSam3
```

均不存在基类。

同时确认公共头文件可以在同一个 `.cpp` 中：

```cpp
#include "trtsam2lib.h"
#include "trtsam3lib.h"
```

而不会发生：

- struct 重定义；
- enum 重定义；
- symbol 冲突；
- SamBase 依赖。

---

## 十二、单库编译验证

分别单独编译：

```text
LVMs/TrtSAM2Lib/TrtSAM2Lib.pro
LVMs/TrtSAM3Lib/TrtSAM3Lib.pro
```

验证：

### 只编译 SAM2

SAM2 不应要求：

```text
TrtSAM3Lib
SamBaseLib
```

存在。

### 只编译 SAM3

SAM3 不应要求：

```text
TrtSAM2Lib
SamBaseLib
```

存在。

---

## 十三、聚合工程编译验证

依次验证：

```text
TrtSAM2Lib.pro
TrtSAM3Lib.pro
LVMs.pro
VisionAIFlowV1.pro
```

至少生成：

```text
BuildLib/
├─ TrtSAM2Lib.dll
├─ TrtSAM2Lib.lib
├─ TrtSAM3Lib.dll
└─ TrtSAM3Lib.lib
```

如果目标仓库要求运行时 DLL 同时放入：

```text
BINX64/
```

则按仓库现有规则复制。

---

## 十四、DLL 依赖检查

使用：

```bat
dumpbin /DEPENDENTS TrtSAM2Lib.dll
dumpbin /DEPENDENTS TrtSAM3Lib.dll
```

确认依赖中不存在：

```text
SamBaseLib.dll
```

同时 SAM2 DLL 不应依赖 SAM3 DLL：

```text
TrtSAM3Lib.dll
```

SAM3 DLL 也不应依赖：

```text
TrtSAM2Lib.dll
```

---

## 十五、功能回归测试

迁移不是算法升级。

对原工程和新工程使用：

- 相同图片；
- 相同 TensorRT engine；
- 相同 Prompt；
- 相同 Point / Rect 输入。

分别测试：

### SAM2

```text
initialize
setCurrentImage

inferByPoint
inferByRect
inferByRects
```

### SAM3

```text
initialize
setCurrentImage

inferByPoint
inferByRect
inferByRects
```

对比：

```text
success
errorMessage

objects.size()

labelName
score

roiData
minRectRoiData
maskContour
```

除正常浮点误差外，行为应与原实现一致。

---

## 十六、禁止事项

Codex 不要执行以下操作：

1. 不要新建 SamBase 替代类。
2. 不要新建 SamCommonLib。
3. 不要让 SAM2 依赖 SAM3。
4. 不要让 SAM3 依赖 SAM2。
5. 不要重写 SAM2/SAM3 TensorRT 推理算法。
6. 不要重写 CUDA kernel。
7. 不要改变模型输入输出定义。
8. 不要修改 mask 计算逻辑。
9. 不要修改 Point/Rect Prompt 行为。
10. 不要继续保留旧工程绝对路径。
11. 不要保留任何 SamBaseLib include/link。
12. 不要因为迁移进行大范围格式化或无关重构。

---

## 十七、最终验收标准

迁移成功必须同时满足：

- [ ] 存在 `LVMs/TrtSAM2Lib`。
- [ ] 存在 `LVMs/TrtSAM3Lib`。
- [ ] `TrtSam2` 不继承 `SamBase`。
- [ ] `TrtSam3` 不继承 `SamBase`。
- [ ] 两个库均不 include `sambaselib.h`。
- [ ] 两个库均不链接 `SamBaseLib.lib`。
- [ ] 两个 DLL 均不依赖 `SamBaseLib.dll`。
- [ ] SAM2/SAM3 结果类型互不冲突。
- [ ] 一个 `.cpp` 可以同时 include SAM2/SAM3 公共头文件。
- [ ] SAM2 可以单独编译。
- [ ] SAM3 可以单独编译。
- [ ] `LVMs.pro` 可以同时编译两库。
- [ ] `VisionAIFlowV1.pro` 完整构建不受影响。
- [ ] CUDA/TensorRT/OpenCV 使用目标仓库统一配置。
- [ ] 无旧工程绝对路径。
- [ ] 原 SAM2 推理功能回归通过。
- [ ] 原 SAM3 推理功能回归通过。

---

## Codex 核心执行原则

> 不要重新设计 SAM2/SAM3，也不要创建新的 Base 类或 Common DLL。  
> `TrtSAM2Lib` 与 `TrtSAM3Lib` 必须分别成为完全自包含的 TensorRT DLL。  
> 任意项目应能够只链接其中一个库，也应允许同一 C++ translation unit 同时 include 两个库的公共头文件而不发生类型重定义。  
> 本次工作重点是工程迁移、SamBaseLib 解耦、构建环境适配和功能等价验证，不进行算法重构。
