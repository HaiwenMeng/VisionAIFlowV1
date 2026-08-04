# ============================================================================
# VisionAIFlowV1 顶层 qmake 工程
#
# 本文件只负责组织子工程、指定各子工程的 .pro 路径，以及描述构建先后依赖。
# 具体的编译选项、第三方依赖路径、输出目录、源文件和链接库由各子工程的
# .pro 与 qmake/common.pri 管理。
#
# 建议按以下层级阅读代码：
# 1. foundation 是无业务倾向的错误与结果基础层。
# 2. domain、annotation、model_graph、tensor、models_common、project_store
#    是领域能力层，保存业务模型、标注、模型描述、资源控制和项目数据。
# 3. yolo11、training、export 是算法实现层，负责检测后处理、训练和导出。
# 4. ipc、qt_foundation 是进程通信与 Host 运行基础设施层。
# 5. app、trainer_host、tensorrt_host、openvino_host、cli 是可执行程序层。
# 6. cpp/tests 下的子工程是针对上述各层的自动化测试层。
#
# 注意：SUBDIRS 中的名字是顶层 qmake 使用的“子工程标识符”，不是库名。
# 每个标识符通过 <标识符>.file 指向真实 .pro，并通过
# <标识符>.depends 声明构建前置关系。
# ============================================================================

# TEMPLATE = subdirs 表示本工程自身不生成 EXE 或 LIB，只聚合并调度子工程。
TEMPLATE = subdirs

# ordered 要求 qmake 按 SUBDIRS 的登记顺序依次构建；它是全量构建的顺序保障。
# 后面的 .depends 还用于表达真实层级，并保证单独构建某个子工程时先构建前置项。
CONFIG += ordered

# ============================================================================
# 一、登记生产模块：基础层与领域能力层
# 重复使用 SUBDIRS += 可让每一行独立对应一个模块，语义等同于反斜杠续行列表。
# ============================================================================

# foundation：全工程最底层静态库，提供 Error、ErrorCode 和 Result<T> 统一错误契约。
SUBDIRS += foundation

# domain：领域状态静态库，目前定义任务 JobState 及状态字符串转换等基础业务语义。
SUBDIRS += domain

# annotation：标注领域静态库，负责几何/坐标变换、标注存储、撤销重做、
# 标签影响分析，以及标签变更事务的写入、校验和应用。
SUBDIRS += annotation

# model_graph：模型图静态库，描述张量形状、节点类型、图节点及模型拓扑校验。
SUBDIRS += model_graph

# tensor：张量与计算资源静态库，目前以 GpuLease 管理 GPU 使用权的生命周期。
SUBDIRS += tensor

# models_common：模型公共静态库，提供检测框、检测结果、letterbox 几何、
# 阈值过滤、坐标还原、NMS 和绘制叠加项等跨模型后处理能力。
SUBDIRS += models_common

# yolo11：YOLO11 专用静态库，将原始检测头输出解码为标准检测结果，
# 并复用 models_common 完成阈值过滤、NMS 与 letterbox 坐标还原。
SUBDIRS += yolo11

# training：LibTorch 训练静态库，包含线性分类训练、TensorDataLoader、AMP、
# checkpoint、异步分类任务，以及 YOLO11 检测模型、损失、指标和增强逻辑。
SUBDIRS += training

# export：模型导出静态库，负责分类/YOLO11 的 ONNX 导出、模型包元数据、
# 校验和、签名状态、插件与许可证清单，以及模型包完整性验证。
SUBDIRS += export

# ============================================================================
# 二、登记基础设施、数据持久化与可执行程序
# ============================================================================

# ipc：进程间通信静态库，定义帧协议、请求 ID 跟踪、QLocalServer 服务端和客户端。
SUBDIRS += ipc

# qt_foundation：Qt Host 公共运行时静态库，提供结构化日志，以及独立 Host 的
# 命令行、握手、心跳、请求分发、异步响应和结构化关闭流程。
SUBDIRS += qt_foundation

# project_store：项目持久化静态库，负责项目定义、原子创建/保存/加载、
# 数据集索引、项目锁、版本迁移和标签定义存储。
SUBDIRS += project_store

# app：主 GUI 程序 VisionAIFlow.exe，负责项目创建/打开、主窗口、标注画布，
# 以及训练、TensorRT、OpenVINO 独立 Host 的启动、监督和关闭。
SUBDIRS += app

# trainer_host：训练进程 VisionTrainerHost.exe，隔离 LibTorch/CUDA 运行时，
# 执行 CUDA 探测、训练任务协调、断点状态和 CUDA 随机数状态管理。
SUBDIRS += trainer_host

# tensorrt_host：TensorRT 推理进程 VisionTensorRtHost.exe，负责 ONNX 校验、
# TensorRT Engine 构建与缓存、分类推理、YOLO11 推理和检测结果解码。
SUBDIRS += tensorrt_host

# openvino_host：OpenVINO 推理进程 VisionOpenVinoHost.exe，负责模型编译、
# 分类推理、YOLO11 推理、运行时元数据采集和检测结果解码。
SUBDIRS += openvino_host

# cli：命令行程序 VisionAIFlowCli.exe；当前 --doctor 用 JSON 报告检查冻结的
# Qt、MSVC、CUDA 和离线依赖路径是否存在。
SUBDIRS += cli

# ============================================================================
# 三、登记自动化测试程序
# 每个条目都会生成独立的 vaf_*_tests.exe，输出到统一 qmake bin 目录。
# ============================================================================

# ipc_tests：验证 IPC 帧编码/拆包、非法帧处理和请求 ID 跟踪行为。
SUBDIRS += ipc_tests

# domain_tests：验证 JobState 的领域状态及其转换行为。
SUBDIRS += domain_tests

# annotation_tests：验证标注几何、坐标映射和相关边界条件。
SUBDIRS += annotation_tests

# annotation_store_tests：验证标注持久化，并覆盖标注与项目存储的协作路径。
SUBDIRS += annotation_store_tests

# annotation_history_tests：验证 AnnotationDocument 的编辑历史、撤销和重做。
SUBDIRS += annotation_history_tests

# tensor_tests：验证 GpuLease 的 GPU 租约获取、持有和释放语义。
SUBDIRS += tensor_tests

# model_graph_tests：验证模型图节点、拓扑关系、形状及图合法性检查。
SUBDIRS += model_graph_tests

# yolo11_tests：综合验证 YOLO11 解码、训练/checkpoint、ONNX/模型包导出，
# 以及 OpenVINO、TensorRT 的端到端检测结果一致性。
SUBDIRS += yolo11_tests

# training_tests：验证 LibTorch 线性分类器的训练、指标和失败路径。
SUBDIRS += training_tests

# tensor_dataloader_tests：验证 TensorDataLoader 的批处理、确定性和参数校验。
SUBDIRS += tensor_dataloader_tests

# amp_tests：在 CPU/通用路径验证自动混合精度控制器的状态与缩放逻辑。
SUBDIRS += amp_tests

# amp_cuda_tests：在真实 CUDA 路径验证 USE_CUDA 下的 AMP 行为和异常报告。
SUBDIRS += amp_cuda_tests

# export_tests：验证 ONNX 导出、模型文件契约和模型包相关失败路径。
SUBDIRS += export_tests

# openvino_host_runtime_tests：启动并验证 OpenVINO Host 的真实运行时、IPC、
# 分类/检测推理和导出模型加载链路。
SUBDIRS += openvino_host_runtime_tests

# project_store_tests：验证项目创建、加载、锁、迁移、数据集索引和标签存储。
SUBDIRS += project_store_tests

# ============================================================================
# 四、子工程标识符到 .pro 文件的映射
# .file 右侧路径相对于本顶层 VisionAIFlowV1.pro 所在目录解析。
# ============================================================================

# foundation 子工程的具体编译规则与源文件清单。
foundation.file = cpp/foundation/foundation.pro
# domain 子工程的具体编译规则与源文件清单。
domain.file = cpp/domain/domain.pro
# annotation 子工程的具体编译规则与源文件清单。
annotation.file = cpp/annotation/annotation.pro
# model_graph 子工程的具体编译规则与源文件清单。
model_graph.file = cpp/model_graph/model_graph.pro
# tensor 子工程的具体编译规则与源文件清单。
tensor.file = cpp/tensor/tensor.pro
# models_common 位于 models/common 二级目录，其 .pro 生成 vaf_models_common 静态库。
models_common.file = cpp/models/common/models_common.pro
# yolo11 位于 models/yolo11 二级目录，其 .pro 生成 vaf_yolo11 静态库。
yolo11.file = cpp/models/yolo11/yolo11.pro
# training 子工程的 LibTorch 训练编译与链接规则。
training.file = cpp/training/training.pro
# export 子工程的 ONNX、Protobuf 与 LibTorch 导出编译和链接规则。
export.file = cpp/export/export.pro
# ipc 子工程的 Qt Core/Network 本地进程通信规则。
ipc.file = cpp/ipc/ipc.pro
# qt_foundation 子工程的 Qt Host 公共运行时与 spdlog 链接规则。
qt_foundation.file = cpp/qt_foundation/qt_foundation.pro
# project_store 子工程的项目文件、数据集、锁、迁移和标签存储规则。
project_store.file = cpp/project_store/project_store.pro
# app 子工程的 Qt Widgets 主程序、头文件和 .ui 表单规则。
app.file = cpp/app/app.pro
# trainer_host 子工程的 LibTorch/CUDA Host 规则，包含 qmake 自定义 nvcc 编译。
trainer_host.file = cpp/trainer_host/trainer_host.pro
# tensorrt_host 子工程的 TensorRT/CUDA 推理 Host 编译和链接规则。
tensorrt_host.file = cpp/tensorrt_host/tensorrt_host.pro
# openvino_host 子工程的 OpenVINO 推理 Host 编译和链接规则。
openvino_host.file = cpp/openvino_host/openvino_host.pro
# cli 子工程的 Qt Core 命令行诊断程序规则。
cli.file = cpp/cli/cli.pro
# IPC 协议测试入口。
ipc_tests.file = cpp/tests/ipc_tests.pro
# 领域任务状态测试入口。
domain_tests.file = cpp/tests/domain_tests.pro
# 标注几何测试入口。
annotation_tests.file = cpp/tests/annotation_tests.pro
# 标注存储测试入口。
annotation_store_tests.file = cpp/tests/annotation_store_tests.pro
# 标注历史测试入口。
annotation_history_tests.file = cpp/tests/annotation_history_tests.pro
# GPU 租约测试入口。
tensor_tests.file = cpp/tests/tensor_tests.pro
# 模型图测试入口。
model_graph_tests.file = cpp/tests/model_graph_tests.pro
# YOLO11 综合测试入口。
yolo11_tests.file = cpp/tests/yolo11_tests.pro
# 训练器测试入口。
training_tests.file = cpp/tests/training_tests.pro
# 张量数据加载器测试入口。
tensor_dataloader_tests.file = cpp/tests/tensor_dataloader_tests.pro
# 通用 AMP 测试入口。
amp_tests.file = cpp/tests/amp_tests.pro
# CUDA AMP 测试入口。
amp_cuda_tests.file = cpp/tests/amp_cuda_tests.pro
# ONNX 与模型包导出测试入口。
export_tests.file = cpp/tests/export_tests.pro
# OpenVINO Host 真实运行时测试入口。
openvino_host_runtime_tests.file = cpp/tests/openvino_host_runtime_tests.pro
# 项目持久化测试入口。
project_store_tests.file = cpp/tests/project_store_tests.pro

# ============================================================================
# 五、生产模块的构建依赖
# A.depends = B C 的含义是：构建 A 前，qmake 必须先成功构建 B 和 C。
# 这里描述的是“构建顺序依赖”；真正的链接依赖仍由 A 自己的 .pro 中 LIBS 决定。
# ============================================================================

# domain 使用 foundation 的 Error/Result 契约，所以 foundation 必须先生成。
domain.depends = foundation
# annotation 的几何、存储和事务接口通过 foundation 返回统一结果与错误。
annotation.depends = foundation
# model_graph 的图创建和校验错误通过 foundation 表达。
model_graph.depends = foundation
# tensor 的 GPU 租约获取与释放结果依赖 foundation。
tensor.depends = foundation
# models_common 的检测后处理参数校验和失败结果依赖 foundation。
models_common.depends = foundation
# yolo11 直接依赖 foundation 的结果类型和 models_common 的通用检测后处理。
yolo11.depends = foundation models_common
# 顶层当前让 training 等待 foundation、model_graph、tensor、models_common。
# 其中 training.pro 当前直接链接 foundation 和 models_common，并未链接
# model_graph、tensor；后两项属于现有的保守构建顺序，重梳层级时可重点复核。
training.depends = foundation model_graph tensor models_common
# export.pro 直接链接 foundation、models_common、training；本行通过 training
# 间接保证 models_common 先构建，同时还保留了当前未直接链接的 model_graph
# 顺序依赖。链接事实以 cpp/export/export.pro 为准，重梳层级时可复核该冗余项。
export.depends = foundation training model_graph
# ipc 使用 foundation 统一返回协议、服务端和客户端错误。
ipc.depends = foundation
# qt_foundation 在 foundation 与 ipc 之上封装 Host 生命周期和结构化日志。
qt_foundation.depends = foundation ipc
# project_store 使用 foundation 报告文件、锁、迁移和数据校验错误。
project_store.depends = foundation
# app 当前显式构建前置项是 foundation、ipc、project_store。
# app.pro 还链接 annotation；全量构建时 ordered 且 annotation 排在 app 前面，
# 因而可用，但若要强化“单独构建 app”的依赖闭包，可另行评估补入 annotation。
app.depends = foundation ipc project_store
# trainer_host 依赖 Host/IPC 基础设施、公共模型与 training；training 的传递前置
# 项会确保 model_graph 和 tensor 也先构建。
trainer_host.depends = foundation ipc qt_foundation models_common training
# tensorrt_host 依赖 Host/IPC 基础设施、公共检测后处理和 YOLO11 解码。
tensorrt_host.depends = foundation ipc qt_foundation models_common yolo11
# openvino_host 与 TensorRT Host 处于并列层级，复用相同的 IPC 和检测解码层。
openvino_host.depends = foundation ipc qt_foundation models_common yolo11

# cli 没有 .depends：其当前实现只使用 Qt Core 和文件系统，不链接项目静态库。

# ============================================================================
# 六、测试程序的构建依赖
# 测试依赖只控制测试 EXE 前应生成哪些被测库/集成模块，不表示测试源码归属。
# ============================================================================

# IPC 测试在 foundation 和 vaf_ipc 已生成后构建。
ipc_tests.depends = foundation ipc
# domain 测试依赖基础错误层与领域状态库。
domain_tests.depends = foundation domain
# 标注几何测试当前只链接 foundation 与 annotation，测试源码也只覆盖 Geometry；
# 本行额外列出的 project_store 是现有冗余构建前置项，重梳层级时可复核或移除。
annotation_tests.depends = foundation annotation project_store
# 标注存储测试覆盖 annotation 与 project_store 的集成。
annotation_store_tests.depends = foundation annotation project_store
# 标注历史只依赖 foundation 与 annotation。
annotation_history_tests.depends = foundation annotation
# GPU 租约测试依赖 foundation 与 tensor。
tensor_tests.depends = foundation tensor
# 模型图测试依赖 foundation 与 model_graph。
model_graph_tests.depends = foundation model_graph
# YOLO11 综合测试需要解码、训练和导出完整链路；training/export 的传递依赖
# 会继续保证 model_graph、tensor 等底层模块已构建。
yolo11_tests.depends = foundation models_common yolo11 training export
# 训练器测试依赖 foundation 与 training，training 自带其底层构建闭包。
training_tests.depends = foundation training
# 数据加载器属于 training，因此测试在 training 之后构建。
tensor_dataloader_tests.depends = foundation training
# 通用 AMP 控制器属于 training，因此测试在 training 之后构建。
amp_tests.depends = foundation training
# CUDA AMP 控制器也属于 training；测试自身 .pro 额外启用 USE_CUDA 并链接 CUDA。
amp_cuda_tests.depends = foundation training
# 导出测试需要训练模型作为输入，并验证 export 的 ONNX/模型包实现。
export_tests.depends = foundation training export
# OpenVINO 运行时测试覆盖从训练、导出、IPC 到 OpenVINO Host 的完整集成路径。
openvino_host_runtime_tests.depends = foundation ipc training export openvino_host
# 项目存储测试依赖 foundation 与 project_store，并由测试 .pro 链接 Windows Advapi32。
project_store_tests.depends = foundation project_store
