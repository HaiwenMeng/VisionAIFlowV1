VisionAIFlowV1
├─ BaseProject（所有的内部编译依赖项）
│     ├─ aiEngine
│     ├─ core
│     ├─ runtime
│     └─ baseModel（把原有的model拆分，基础父类和api放到这里，具体实现放到ModelPlugins）
│           ├─ models_Common
│           ├─ models_builtins
│           ├─ 。。。。
│           └─ models_api
│ 
├─ SoftwareProject(所有的app)
│  ├─ app
│  ├─ cli
│  ├─ ....
│  └─ openvino_host
│
├─ ModelPlugins(所有的模型具体实现动态加载dll示例)
│  ├─ Detection
│  │    ├─ yolo11
│  │    ├─ RFDert
│  │    └─ ....
│  ├─ Classify
│       └─ ....
├─ TestDemo(所有的测试DEMO)
│  ├─ integration
│  │   └─ ...
│  │ 
│  └─ unit
│     ├─ ...
│     └─ ....
