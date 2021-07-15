set(TARGET_NAME questInterface)
setup_hifi_library()
link_hifi_libraries(
    shared task networking qml
    image model-serializers hfm render-utils physics entities octree
    oculusMobile oculusMobilePlugin
    gl gpu ${PLATFORM_GL_BACKEND}
)
target_opengl()
target_bullet()
target_oculus_mobile()

add_subdirectory("${CMAKE_SOURCE_DIR}/interface" "libraries/interface")
include_directories("${CMAKE_SOURCE_DIR}/interface/src")
target_link_libraries(questInterface android log m interface)
