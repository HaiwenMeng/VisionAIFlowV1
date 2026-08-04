add_executable(VisionTensorRtHost src/main.cpp)
target_link_libraries(VisionTensorRtHost PRIVATE vaf::qt_foundation TensorRT::nvinfer TensorRT::nvonnxparser)
