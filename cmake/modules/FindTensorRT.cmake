include(FindPackageHandleStandardArgs)

set(TensorRT_ROOT "${VISIONAIFLOW_TENSORRT_ROOT}" CACHE PATH "Frozen TensorRT SDK root")
find_path(TensorRT_INCLUDE_DIR NvInferVersion.h PATHS "${TensorRT_ROOT}/include" NO_DEFAULT_PATH)
find_library(TensorRT_NVINFER_LIBRARY NAMES nvinfer_10 nvinfer PATHS "${TensorRT_ROOT}/lib" NO_DEFAULT_PATH)
find_library(TensorRT_NVPARSER_LIBRARY NAMES nvonnxparser_10 nvonnxparser PATHS "${TensorRT_ROOT}/lib" NO_DEFAULT_PATH)

find_package_handle_standard_args(TensorRT REQUIRED_VARS TensorRT_INCLUDE_DIR TensorRT_NVINFER_LIBRARY TensorRT_NVPARSER_LIBRARY)

if(TensorRT_FOUND AND NOT TARGET TensorRT::nvinfer)
    add_library(TensorRT::nvinfer UNKNOWN IMPORTED)
    set_target_properties(TensorRT::nvinfer PROPERTIES
        IMPORTED_LOCATION "${TensorRT_NVINFER_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${TensorRT_INCLUDE_DIR}")
    add_library(TensorRT::nvonnxparser UNKNOWN IMPORTED)
    set_target_properties(TensorRT::nvonnxparser PROPERTIES
        IMPORTED_LOCATION "${TensorRT_NVPARSER_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${TensorRT_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES TensorRT::nvinfer)
endif()
