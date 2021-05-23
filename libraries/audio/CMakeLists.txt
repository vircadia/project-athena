set(TARGET_NAME audio)
setup_hifi_library(Network)

if (ANDROID)
  add_definitions("-D__STDC_CONSTANT_MACROS")
endif ()

link_hifi_libraries(networking shared plugins script-engine)
