add_executable(VisionOpenVinoHost src/main.cpp)
target_link_libraries(VisionOpenVinoHost PRIVATE vaf::qt_foundation openvino::runtime)
