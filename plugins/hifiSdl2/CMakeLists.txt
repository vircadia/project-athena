#
#  Created by Bradley Austin Davis on 2015/11/18
#  Copyright 2015 High Fidelity, Inc.
#
#  Distributed under the Apache License, Version 2.0.
#  See the accompanying file LICENSE or http:#www.apache.org/licenses/LICENSE-2.0.html
#

if (NOT APPLE)
    set(TARGET_NAME hifiSdl2)
    if (CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 10.0)
        # GCC 10 and above sets -fno-common by default, and causes a linking problem here:
        # multiple definition of `WAYLAND_wl_proxy_marshal'
        #
        # Work around it per https://medium.com/@clentfort/using-esy-sdl2-with-gcc-10-91b4fa0c5aa9
        link_libraries("-Wl,--allow-multiple-definition")
    endif()
    setup_hifi_plugin(Qml)
    link_hifi_libraries(shared controllers ui plugins input-plugins)
    include_hifi_library_headers(script-engine)
    target_sdl2()
endif()
