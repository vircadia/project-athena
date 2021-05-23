
# Declare dependencies
macro (SETUP_TESTCASE_DEPENDENCIES)
  target_bullet()
  link_hifi_libraries(shared test-utils physics gpu graphics)
  package_libraries_for_deployment()
endmacro ()

setup_hifi_testcase()
