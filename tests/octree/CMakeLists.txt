
# Declare dependencies
macro (setup_testcase_dependencies)
  # link in the shared libraries
  link_hifi_libraries(shared test-utils octree gpu graphics fbx networking entities avatars audio animation script-engine physics)

  package_libraries_for_deployment()
endmacro ()

setup_hifi_testcase(Network)
