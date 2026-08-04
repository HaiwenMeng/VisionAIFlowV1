function(vaf_verify_cuda_toolchain)
    if(NOT DEFINED CUDAToolkit_ROOT OR CUDAToolkit_ROOT STREQUAL "")
        message(FATAL_ERROR "CUDAToolkit_ROOT must explicitly point to CUDA 11.8.89. CUDA discovery through PATH is prohibited.")
    endif()

    set(_vaf_nvcc "${CUDAToolkit_ROOT}/bin/nvcc.exe")
    if(NOT EXISTS "${_vaf_nvcc}")
        message(FATAL_ERROR "CUDA 11.8.89 nvcc was not found at '${_vaf_nvcc}'.")
    endif()

    execute_process(
        COMMAND "${_vaf_nvcc}" --version
        RESULT_VARIABLE _vaf_nvcc_result
        OUTPUT_VARIABLE _vaf_nvcc_output
        ERROR_VARIABLE _vaf_nvcc_error)
    if(NOT _vaf_nvcc_result EQUAL 0 OR NOT _vaf_nvcc_output MATCHES "release 11\\.8, V11\\.8\\.89")
        message(FATAL_ERROR "VisionAIFlowV1 requires CUDA Toolkit 11.8.89. nvcc output: ${_vaf_nvcc_output}${_vaf_nvcc_error}")
    endif()
endfunction()
