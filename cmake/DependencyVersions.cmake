set(VAF_QT_VERSION "6.9.2")
set(VAF_LIBTORCH_VERSION "2.7.1-cu118")
set(VAF_LIBTORCH_VERSION_NUMBER "2.7.1")
set(VAF_TENSORRT_VERSION "10.0.1.6-cuda11.8")
set(VAF_OPENVINO_VERSION "2025.3.0")
set(VAF_OPENCV_VERSION "4.12.0")
set(VAF_ONNX_VERSION "1.15.0")
set(VAF_PROTOBUF_VERSION "3.20.3")
set(VAF_NLOHMANN_JSON_VERSION "3.12.0")
set(VAF_SPDLOG_VERSION "1.15.3")
set(VAF_GTEST_VERSION "1.17.0")

function(vaf_verify_dependency_layout)
    if(NOT DEFINED VISIONAIFLOW_DEPS_ROOT OR VISIONAIFLOW_DEPS_ROOT STREQUAL "")
        message(FATAL_ERROR "VISIONAIFLOW_DEPS_ROOT is required and must point to F:/VisionAIFlowDeps. System PATH discovery is prohibited.")
    endif()

    if(NOT DEFINED VISIONAIFLOW_DEP_CONFIGURATION OR NOT VISIONAIFLOW_DEP_CONFIGURATION MATCHES "^(Debug|Release)$")
        message(FATAL_ERROR "VISIONAIFLOW_DEP_CONFIGURATION must be Debug or Release; configure through a QT6_MSVC2022 preset.")
    endif()
    string(TOLOWER "${VISIONAIFLOW_DEP_CONFIGURATION}" _vaf_torch_configuration)

    set(_vaf_required_paths
        "${VISIONAIFLOW_DEPS_ROOT}/libtorch/${VAF_LIBTORCH_VERSION}/${_vaf_torch_configuration}/share/cmake/Torch/TorchConfig.cmake"
        "${VISIONAIFLOW_DEPS_ROOT}/libtorch/${VAF_LIBTORCH_VERSION}/debug/share/cmake/Torch/TorchConfig.cmake"
        "${VISIONAIFLOW_TENSORRT_ROOT}/include/NvInferVersion.h"
        "${VISIONAIFLOW_OPENVINO_ROOT}/include/openvino/openvino.hpp"
        "${VISIONAIFLOW_DEPS_ROOT}/install/${VISIONAIFLOW_DEP_CONFIGURATION}/include/opencv2/core/version.hpp"
        "${VISIONAIFLOW_DEPS_ROOT}/build/opencv-${VAF_OPENCV_VERSION}-${VISIONAIFLOW_DEP_CONFIGURATION}-offline/OpenCVConfig.cmake"
        "${VISIONAIFLOW_DEPS_ROOT}/install/${VISIONAIFLOW_DEP_CONFIGURATION}/include/onnx/common/version.h"
        "${VISIONAIFLOW_DEPS_ROOT}/install/${VISIONAIFLOW_DEP_CONFIGURATION}/include/google/protobuf/stubs/common.h"
        "${VISIONAIFLOW_DEPS_ROOT}/nlohmann_json/${VAF_NLOHMANN_JSON_VERSION}/include/nlohmann/json.hpp"
        "${VISIONAIFLOW_DEPS_ROOT}/install/${VISIONAIFLOW_DEP_CONFIGURATION}/include/spdlog/version.h"
        "${VISIONAIFLOW_DEPS_ROOT}/install/${VISIONAIFLOW_DEP_CONFIGURATION}/include/gtest/gtest.h")

    set(_vaf_missing_paths "")
    foreach(_vaf_path IN LISTS _vaf_required_paths)
        if(NOT EXISTS "${_vaf_path}")
            list(APPEND _vaf_missing_paths "${_vaf_path}")
        endif()
    endforeach()
    if(_vaf_missing_paths)
        list(JOIN _vaf_missing_paths "\n  - " _vaf_missing_text)
        message(FATAL_ERROR "Frozen offline dependencies are missing or incomplete. Expected versions are recorded in config/dependencies.lock.json. Missing paths:\n  - ${_vaf_missing_text}\nPopulate only the declared F:/VisionAIFlowDeps locations, verify SHA-256 values, then rerun tools/deps/verify-deps.ps1. No network download or PATH fallback is allowed.")
    endif()
endfunction()
