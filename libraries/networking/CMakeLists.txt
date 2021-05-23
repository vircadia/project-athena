set(TARGET_NAME networking)
setup_hifi_library(Network)
link_hifi_libraries(shared platform)
include_hifi_library_headers(script-engine)

target_openssl()
target_tbb()

if (WIN32)
    # we need ws2_32.lib on windows, but it's static so we don't bubble it up
    target_link_libraries(${TARGET_NAME} ws2_32.lib)
elseif(APPLE)
    # IOKit is needed for getting machine fingerprint
    find_library(FRAMEWORK_IOKIT IOKit)
    find_library(CORE_FOUNDATION CoreFoundation)
    target_link_libraries(${TARGET_NAME} ${FRAMEWORK_IOKIT} ${CORE_FOUNDATION})
elseif(UNIX)
    # libcrypto uses dlopen in libdl
    target_link_libraries(${TARGET_NAME} ${CMAKE_DL_LIBS})
endif ()
