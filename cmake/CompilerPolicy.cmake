function(vaf_verify_compiler_policy)
    if(NOT CMAKE_GENERATOR_PLATFORM STREQUAL "x64")
        message(FATAL_ERROR "VisionAIFlowV1 requires the x64 generator platform; got '${CMAKE_GENERATOR_PLATFORM}'.")
    endif()

    if(NOT MSVC)
        message(FATAL_ERROR "VisionAIFlowV1 requires MSVC v143 14.36 (compiler 19.36.x); detected '${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}'.")
    endif()

    if(NOT MSVC_VERSION EQUAL 1936)
        message(FATAL_ERROR "VisionAIFlowV1 requires MSVC v143 14.36 / compiler 19.36.x for CUDA 11.8.89. Detected compiler '${CMAKE_CXX_COMPILER}' version '${CMAKE_CXX_COMPILER_VERSION}' (MSVC_VERSION=${MSVC_VERSION}). Install 14.36 side-by-side and configure preset QT6_MSVC2022-Release again. Unsupported compiler overrides are prohibited.")
    endif()

    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "VisionAIFlowV1 requires a 64-bit build; CMAKE_SIZEOF_VOID_P=${CMAKE_SIZEOF_VOID_P}.")
    endif()

    if(MSVC)
        add_compile_options(/W4 /WX /permissive-)
    endif()
endfunction()
