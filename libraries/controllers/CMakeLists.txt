set(TARGET_NAME controllers)

# set a default root dir for each of our optional externals if it was not passed
setup_hifi_library(Qml)

# use setup_hifi_library macro to setup our project and link appropriate Qt modules
link_hifi_libraries(shared script-engine)
include_hifi_library_headers(networking)

GroupSources("src/controllers")
