cmake_minimum_required(VERSION 3.30.5)

project(VisionAIFlowV1 VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(FETCHCONTENT_FULLY_DISCONNECTED ON CACHE BOOL "Disable network dependency downloads" FORCE)
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

include(cmake/CompilerPolicy.cmake)
include(cmake/DependencyVersions.cmake)
include(cmake/modules/VerifyCudaToolchain.cmake)

vaf_verify_compiler_policy()
vaf_verify_cuda_toolchain()
vaf_verify_dependency_layout()

if(NOT DEFINED VISIONAIFLOW_DEP_CONFIGURATION OR NOT VISIONAIFLOW_DEP_CONFIGURATION MATCHES "^(Debug|Release)$")
    message(FATAL_ERROR "VISIONAIFLOW_DEP_CONFIGURATION must be Debug or Release; configure through a QT6_MSVC2022 preset.")
endif()

find_package(Qt6 ${VAF_QT_VERSION} EXACT REQUIRED COMPONENTS Core Network Widgets Test)
string(TOLOWER "${VISIONAIFLOW_DEP_CONFIGURATION}" _vaf_torch_configuration)
find_package(Torch ${VAF_LIBTORCH_VERSION_NUMBER} EXACT REQUIRED CONFIG PATHS "${VISIONAIFLOW_DEPS_ROOT}/libtorch/${VAF_LIBTORCH_VERSION}/${_vaf_torch_configuration}/share/cmake/Torch" NO_DEFAULT_PATH)
find_package(OpenVINO REQUIRED)
find_package(OpenCV ${VAF_OPENCV_VERSION} EXACT REQUIRED CONFIG PATHS "${VISIONAIFLOW_DEPS_ROOT}/build/opencv-${VAF_OPENCV_VERSION}-${VISIONAIFLOW_DEP_CONFIGURATION}-offline" NO_DEFAULT_PATH)
find_package(Protobuf ${VAF_PROTOBUF_VERSION} EXACT REQUIRED CONFIG PATHS "${VISIONAIFLOW_DEPS_ROOT}/install/${VISIONAIFLOW_DEP_CONFIGURATION}/cmake" NO_DEFAULT_PATH)
find_package(spdlog ${VAF_SPDLOG_VERSION} EXACT REQUIRED CONFIG PATHS "${VISIONAIFLOW_DEPS_ROOT}/install/${VISIONAIFLOW_DEP_CONFIGURATION}/lib/cmake/spdlog" NO_DEFAULT_PATH)
find_package(TensorRT REQUIRED)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}")
execute_process(COMMAND git -C "${CMAKE_SOURCE_DIR}" rev-parse HEAD OUTPUT_VARIABLE VAF_GIT_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
file(WRITE "${CMAKE_BINARY_DIR}/resolved-dependencies.json"
    "{\n"
    "  \"compiler\": \"${CMAKE_CXX_COMPILER}\",\n"
    "  \"compilerVersion\": \"${CMAKE_CXX_COMPILER_VERSION}\",\n"
    "  \"qtVersion\": \"${Qt6_VERSION}\",\n"
    "  \"cudaRoot\": \"${CUDAToolkit_ROOT}\",\n"
    "  \"libtorchVersion\": \"${VAF_LIBTORCH_VERSION}\",\n"
    "  \"tensorRtVersion\": \"${VAF_TENSORRT_VERSION}\",\n"
    "  \"onnxVersion\": \"${VAF_ONNX_VERSION}\",\n"
    "  \"onnxOpset\": 12,\n"
    "  \"protobufVersion\": \"${VAF_PROTOBUF_VERSION}\",\n"
    "  \"openVinoVersion\": \"${VAF_OPENVINO_VERSION}\",\n"
    "  \"openCvVersion\": \"${VAF_OPENCV_VERSION}\",\n"
    "  \"gitCommit\": \"${VAF_GIT_COMMIT}\"\n"
    "}\n")

add_subdirectory(cpp/core/foundation)
add_subdirectory(cpp/core/domain)
add_subdirectory(cpp/data/project_store)
add_subdirectory(cpp/runtime/ipc)
add_subdirectory(cpp/runtime/qt_foundation)
add_subdirectory(cpp/apps/app)
add_subdirectory(cpp/apps/hosts/trainer)
add_subdirectory(cpp/apps/hosts/tensorrt)
add_subdirectory(cpp/apps/hosts/openvino)
add_subdirectory(cpp/apps/cli)
add_subdirectory(tests)
