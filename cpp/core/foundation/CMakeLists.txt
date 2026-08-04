add_library(vaf_foundation STATIC
    src/Error.cpp)

add_library(vaf::foundation ALIAS vaf_foundation)
target_include_directories(vaf_foundation PUBLIC include)
target_compile_features(vaf_foundation PUBLIC cxx_std_20)
