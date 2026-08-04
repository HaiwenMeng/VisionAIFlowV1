add_library(vaf_domain STATIC
    src/JobState.cpp)

add_library(vaf::domain ALIAS vaf_domain)
target_include_directories(vaf_domain PUBLIC include)
target_link_libraries(vaf_domain PUBLIC vaf::foundation)
target_compile_features(vaf_domain PUBLIC cxx_std_20)
