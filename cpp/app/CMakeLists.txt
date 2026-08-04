add_executable(VisionAIFlow
    main.cpp
    src/MainWindow.cpp
    src/CreateProjectDialog.cpp
    src/HostSupervisor.cpp
    forms/MainWindow.ui
    forms/CreateProjectDialog.ui)

target_include_directories(VisionAIFlow PRIVATE include)
target_link_libraries(VisionAIFlow PRIVATE vaf::foundation vaf::ipc vaf::project_store Qt6::Core Qt6::Network Qt6::Widgets)
set_target_properties(VisionAIFlow PROPERTIES AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/forms")
