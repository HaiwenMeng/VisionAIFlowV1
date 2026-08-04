# VisionAIFlowV1

Windows x64 离线视觉标注、训练、导出和推理产品。唯一正式技术路线为 Qt 6.9.2、C++20、LibTorch 2.7.1 cu118、ONNX opset 12、TensorRT 10.0.1.6 和 OpenVINO 2025.3.0。

## 当前状态

Phase 0-1 已完成并通过双配置验证：MSVC 14.36、CUDA 11.8、Qt 6.9.2、LibTorch 2.7.1 cu118、TensorRT 10.0.1.6、OpenVINO 2025.3.0 与所有锁定的离线源码依赖均已校验。基础程序、三种独立 Host、IPC、结构化日志、项目创建/原子写入/项目锁及单元测试已建立。

用户已明确指定 qmake/.pro 为唯一当前构建入口。根工程为 `VisionAIFlowV1.pro`，CUDA 源码由 qmake 自定义编译规则直接调用 CUDA 11.8 的 `nvcc`，不使用 Visual Studio Integration 或 Build Customizations。保留的 CMake 文件不是产品构建入口。

## Qt Creator 20 构建

1. 在 Qt Creator 20 打开 `F:\VisionAIFlowV1\VisionAIFlowV1.pro`。
2. 选择唯一 Kit：Qt 6.9.2 MSVC2022 x64，并选择 Release 或 Debug。
3. 执行 qmake，然后构建根工程。`CudaRuntimeProbe.cu` 会由 `nvcc 11.8.89` 使用冻结的 MSVC 14.36 编译。
4. 输出分别位于 `out\qmake\Release` 与 `out\qmake\Debug`。

命令行等价入口：

```bat
tools\build-qmake-release.cmd
tools\build-qmake-debug.cmd
```

构建前可验证离线依赖：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\deps\verify-deps.ps1 -Configuration Release -DepsRoot F:\VisionAIFlowDeps
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\deps\verify-deps.ps1 -Configuration Debug -DepsRoot F:\VisionAIFlowDeps
```

单元测试输出为 `out\qmake\<Configuration>\bin\vaf_*_tests.exe`。运行时须将 Qt 6.9.2 的 `bin` 目录置于 `PATH`。

Release 构建后可审计 UI 进程隔离，确认 `VisionAIFlow.exe` 没有导入 LibTorch、CUDA、TensorRT 或 OpenVINO Runtime：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\audit-ui-runtime.ps1 -ExecutablePath .\out\qmake\Release\bin\VisionAIFlow.exe -BuildToolsRoot F:\VS2022\BuildTools -MsvcVersion 14.36.32532
```

旧工程目录只可读取，严禁在其中写入、移动或删除文件。
