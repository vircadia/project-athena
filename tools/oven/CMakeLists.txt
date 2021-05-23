set(TARGET_NAME oven)

setup_hifi_project(Widgets Gui Concurrent)

link_hifi_libraries(shared shaders image gpu ktx model-serializers hfm baking graphics networking procedural material-networking model-baker task)
include_hifi_library_headers(script-engine)

setup_memory_debugger()
setup_thread_debugger()

if (WIN32)
  package_libraries_for_deployment()
elseif (UNIX AND NOT APPLE)
  find_package(Threads REQUIRED)
  if(THREADS_HAVE_PTHREAD_ARG)
    target_compile_options(oven PUBLIC "-pthread")
  endif()
elseif (APPLE)
  # Fix up the rpath so macdeployqt works
  set_target_properties(${TARGET_NAME} PROPERTIES INSTALL_RPATH "@executable_path/../Frameworks")
endif()

install_beside_console()
