set(TARGET_NAME render-utils)

# pull in the resources.qrc file
qt5_add_resources(QT_RESOURCES_FILE "${CMAKE_CURRENT_SOURCE_DIR}/res/fonts/fonts.qrc")
setup_hifi_library(Gui Network Qml Quick Script)
link_hifi_libraries(shared task ktx gpu shaders graphics graphics-scripting material-networking model-networking render animation model-serializers image procedural)
include_hifi_library_headers(audio)
include_hifi_library_headers(networking)
include_hifi_library_headers(octree)
include_hifi_library_headers(hfm)

# tell CMake to exclude qrc_fonts.cpp for policy CMP0071
set_property(SOURCE qrc_fonts.cpp PROPERTY SKIP_AUTOMOC ON)

if (NOT ANDROID)
  target_nsight()
endif ()

if (WIN32)
  add_compile_definitions(_USE_MATH_DEFINES)
endif()
