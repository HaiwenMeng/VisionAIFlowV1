add_library(vaf_ipc STATIC
    src/Protocol.cpp
    src/LocalServer.cpp
    src/LocalClient.cpp)

add_library(vaf::ipc ALIAS vaf_ipc)
target_include_directories(vaf_ipc PUBLIC include)
target_link_libraries(vaf_ipc PUBLIC vaf::foundation Qt6::Core Qt6::Network)
target_compile_features(vaf_ipc PUBLIC cxx_std_20)
