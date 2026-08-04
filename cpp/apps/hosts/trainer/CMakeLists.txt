add_executable(VisionTrainerHost src/main.cpp)
target_link_libraries(VisionTrainerHost PRIVATE vaf::qt_foundation Torch::Torch)
