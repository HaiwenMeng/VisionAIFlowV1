include(FindPackageHandleStandardArgs)

set(OpenVINO_ROOT "${VISIONAIFLOW_OPENVINO_ROOT}" CACHE PATH "Frozen OpenVINO runtime root")
find_path(OpenVINO_INCLUDE_DIR openvino/openvino.hpp PATHS "${OpenVINO_ROOT}/include" NO_DEFAULT_PATH)
find_library(OpenVINO_LIBRARY NAMES openvino PATHS "${OpenVINO_ROOT}/lib" NO_DEFAULT_PATH)
find_package_handle_standard_args(OpenVINO REQUIRED_VARS OpenVINO_INCLUDE_DIR OpenVINO_LIBRARY)

if(OpenVINO_FOUND AND NOT TARGET openvino::runtime)
    add_library(openvino::runtime UNKNOWN IMPORTED)
    set_target_properties(openvino::runtime PROPERTIES
        IMPORTED_LOCATION "${OpenVINO_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${OpenVINO_INCLUDE_DIR}")
endif()
