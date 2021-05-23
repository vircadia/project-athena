set(TARGET_NAME gpu-test)
# This is not a testcase -- just set it up as a regular hifi project
setup_hifi_project(Quick Gui)
setup_memory_debugger()
setup_thread_debugger()
set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "Tests/manual-tests/")
link_hifi_libraries(
    shared task networking gl 
    ktx shaders gpu procedural octree image 
    graphics model-networking fbx hfm animation material-networking
    script-engine render render-utils 
    ${PLATFORM_GL_BACKEND}
)

if (WIN32)
  add_dependency_external_projects(wasapi)
endif ()

package_libraries_for_deployment()

target_nsight()
