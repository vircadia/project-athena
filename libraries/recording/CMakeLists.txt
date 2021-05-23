set(TARGET_NAME recording)

# set a default root dir for each of our optional externals if it was not passed
setup_hifi_library()

# use setup_hifi_library macro to setup our project and link appropriate Qt modules
link_hifi_libraries(shared networking)

GroupSources("src/recording")
