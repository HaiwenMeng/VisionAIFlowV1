add_executable(vaf_ipc_tests
    unit/tst_IpcProtocol.cpp)

target_link_libraries(vaf_ipc_tests PRIVATE vaf::foundation vaf::ipc Qt6::Core Qt6::Test)
add_test(NAME vaf_ipc_tests COMMAND vaf_ipc_tests)
set_tests_properties(vaf_ipc_tests PROPERTIES LABELS "unit;ipc")

add_executable(vaf_domain_tests
    unit/tst_JobState.cpp)

target_link_libraries(vaf_domain_tests PRIVATE vaf::foundation vaf::domain Qt6::Core Qt6::Test)
add_test(NAME vaf_domain_tests COMMAND vaf_domain_tests)
set_tests_properties(vaf_domain_tests PROPERTIES LABELS "unit;domain")

add_executable(vaf_project_store_tests unit/tst_ProjectStore.cpp)
target_link_libraries(vaf_project_store_tests PRIVATE vaf::foundation vaf::project_store Qt6::Core Qt6::Test)
add_test(NAME vaf_project_store_tests COMMAND vaf_project_store_tests)
set_tests_properties(vaf_project_store_tests PROPERTIES LABELS "unit;project_store")
