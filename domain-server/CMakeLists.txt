set(TARGET_NAME domain-server)

if (UPPER_CMAKE_BUILD_TYPE MATCHES DEBUG AND NOT WIN32)
  set(_SHOULD_SYMLINK_RESOURCES TRUE)
else ()
  set(_SHOULD_SYMLINK_RESOURCES FALSE)
endif ()

# setup the project and link required Qt modules
setup_hifi_project(Network)

# Fix up the rpath so macdeployqt works
if (APPLE)
  set_target_properties(${TARGET_NAME} PROPERTIES INSTALL_RPATH "@executable_path/../Frameworks")
endif ()

setup_memory_debugger()
setup_thread_debugger()

# TODO: find a solution that will handle web file changes in resources on windows without a re-build.
# Currently the resources are only copied on post-build. If one is changed but the domain-server is not, they will
# not be re-copied. This is worked-around on OS X/UNIX by using a symlink.
symlink_or_copy_directory_beside_target(${_SHOULD_SYMLINK_RESOURCES} "${CMAKE_CURRENT_SOURCE_DIR}/resources" "resources")

# link the shared hifi libraries
include_hifi_library_headers(gpu)
include_hifi_library_headers(graphics)
include_hifi_library_headers(script-engine)
link_hifi_libraries(embedded-webserver networking shared avatars octree)

target_zlib()
target_quazip()
target_openssl()

# libcrypto uses dlopen in libdl
if (UNIX)
  target_link_libraries(${TARGET_NAME} ${CMAKE_DL_LIBS})
endif ()

if (WIN32)
  package_libraries_for_deployment()
endif ()

install_beside_console()
