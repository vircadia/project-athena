# FIXME Disabling test on OSX because of unexpected link error 
if (NOT APPLE)
set(TARGET_NAME controllers-test)
 
# This is not a testcase -- just set it up as a regular hifi project
setup_hifi_project(Qml)
set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "Tests/manual-tests/")

setup_memory_debugger()
setup_thread_debugger()

# link in the shared libraries
link_hifi_libraries(shared gl script-engine plugins render-utils ui-plugins input-plugins display-plugins controllers)

if (WIN32)
    target_openvr()
    target_link_libraries(${TARGET_NAME} Winmm.lib)
    add_dependency_external_projects(wasapi)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_link_libraries(${TARGET_NAME} atomic)
endif()

package_libraries_for_deployment()
endif()
