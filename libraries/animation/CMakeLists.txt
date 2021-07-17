set(TARGET_NAME animation)
setup_hifi_library(Network Script)
link_hifi_libraries(shared graphics model-serializers)
include_hifi_library_headers(networking)
include_hifi_library_headers(gpu)
include_hifi_library_headers(hfm)
include_hifi_library_headers(image)

target_nsight()

if (WIN32)
  add_compile_definitions(_USE_MATH_DEFINES)
endif()
