# add the tool directories
message(STATUS "Checking for npm")
find_npm()

if (NPM_EXECUTABLE)
    message(STATUS "Checking for npm - found ${NPM_EXECUTABLE}, will build jsdoc")
    add_subdirectory(jsdoc)
    set_target_properties(jsdoc PROPERTIES FOLDER "Tools")
else()
    message(NOTICE "Checking for npm - not found, jsdoc won't be genereated. Tab completion in the js console won't work!")
endif()

function(check_test name)
    set(RESULT TRUE)
    if (BUILD_TOOLS_INCLUDE)
        unset(RESULT)
        list(FIND BUILD_TOOLS_INCLUDE ${name} BUILD_TOOL_FIND)
        if (NOT (${BUILD_TOOL_FIND} EQUAL -1))
            set(RESULT TRUE)
        endif()
    endif()
    set(BUILD_TOOL_RESULT ${RESULT} PARENT_SCOPE)
endfunction()

if (BUILD_TOOLS)
    set(ALL_TOOLS 
        udt-test
        gpu-frame-player
        ice-client
        ktx-tool
        ac-client
        skeleton-dump
        atp-client
    )

    # Don't include oven or vhacd-til in OSX client-only DMGs.
    if (NOT CLIENT_ONLY OR NOT APPLE)
        list(APPEND ALL_TOOLS oven)
        list(APPEND ALL_TOOLS vhacd-util)
    endif()

    # Allow different tools for stable builds
    if (STABLE_BUILD)
    else()
        # Don't include nitpick in OSX client-only DMGs.
        if (NOT APPLE OR NOT CLIENT_ONLY)
            list(APPEND ALL_TOOLS nitpick)
        endif()
    endif()

    foreach(TOOL ${ALL_TOOLS})
        check_test(${TOOL})
        if (${BUILD_TOOL_RESULT})
            add_subdirectory(${TOOL})
            if (TARGET ${TOOL})
                set_target_properties(${TOOL} PROPERTIES FOLDER "Tools")
            endif()
        endif()
    endforeach()
endif()
