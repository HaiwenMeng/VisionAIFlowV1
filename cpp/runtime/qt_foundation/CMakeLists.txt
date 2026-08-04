add_library(vaf_qt_foundation STATIC
    src/StructuredLogger.cpp
    src/HostRuntime.cpp)

add_library(vaf::qt_foundation ALIAS vaf_qt_foundation)
target_include_directories(vaf_qt_foundation PUBLIC include)
target_link_libraries(vaf_qt_foundation PUBLIC vaf::foundation vaf::ipc Qt6::Core spdlog::spdlog)
target_compile_features(vaf_qt_foundation PUBLIC cxx_std_20)
