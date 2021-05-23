
set(TARGET_NAME "entities-test")

# This is not a testcase -- just set it up as a regular hifi project
setup_hifi_project(Network)
setup_memory_debugger()
setup_thread_debugger()
set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "Tests/manual-tests/")

# link in the shared libraries
link_hifi_libraries(entities avatars shared octree gpu graphics fbx networking animation audio gl)

if (WIN32)
  add_dependency_external_projects(wasapi)
endif ()

package_libraries_for_deployment()
