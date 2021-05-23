if (APPLE)
  # Make the OSX DMG app be Vircadia.app.
  set(TARGET_NAME Vircadia)
else()
  set(TARGET_NAME interface)
endif()

project(${TARGET_NAME})

file(GLOB_RECURSE QML_SRC resources/qml/*.qml resources/qml/*.js)
add_custom_target(qmls SOURCES ${QML_SRC})
GroupSources("resources/qml")

function(JOIN VALUES GLUE OUTPUT)
  string (REGEX REPLACE "([^\\]|^);" "\\1${GLUE}" _TMP_STR "${VALUES}")
  string (REGEX REPLACE "[\\](.)" "\\1" _TMP_STR "${_TMP_STR}") #fixes escaping
  set (${OUTPUT} "${_TMP_STR}" PARENT_SCOPE)
endfunction()

set(CUSTOM_INTERFACE_QRC_PATHS "")

find_npm()

if (BUILD_TOOLS AND NPM_EXECUTABLE)
  add_custom_qrc_path(CUSTOM_INTERFACE_QRC_PATHS "${CMAKE_SOURCE_DIR}/tools/jsdoc/out/hifiJSDoc.json" "auto-complete/hifiJSDoc.json")
endif ()

set(RESOURCES_QRC ${CMAKE_CURRENT_BINARY_DIR}/resources.qrc)
set(RESOURCES_RCC ${CMAKE_CURRENT_SOURCE_DIR}/compiledResources/resources.rcc)
generate_qrc(OUTPUT ${RESOURCES_QRC} PATH ${CMAKE_CURRENT_SOURCE_DIR}/resources CUSTOM_PATHS ${CUSTOM_INTERFACE_QRC_PATHS} GLOBS *)

if (ANDROID)
  # on Android, don't compress the rcc binary
  add_custom_command(
    OUTPUT ${RESOURCES_RCC}
    DEPENDS ${RESOURCES_QRC} ${GENERATE_QRC_DEPENDS}
    COMMAND "${QT_DIR}/bin/rcc"
    ARGS ${RESOURCES_QRC} -no-compress -binary -o ${RESOURCES_RCC}
  )
else ()
  add_custom_command(
    OUTPUT ${RESOURCES_RCC}
    DEPENDS ${RESOURCES_QRC} ${GENERATE_QRC_DEPENDS}
    COMMAND "${QT_DIR}/bin/rcc"
    ARGS ${RESOURCES_QRC} -binary -o ${RESOURCES_RCC}
  )
endif()

list(APPEND GENERATE_QRC_DEPENDS ${RESOURCES_RCC})
add_custom_target(resources ALL DEPENDS ${GENERATE_QRC_DEPENDS})

# set a default root dir for each of our optional externals if it was not passed
set(OPTIONAL_EXTERNALS "LeapMotion")

setup_memory_debugger()
setup_thread_debugger()

foreach(EXTERNAL ${OPTIONAL_EXTERNALS})
  string(TOUPPER ${EXTERNAL} ${EXTERNAL}_UPPERCASE)
  if (NOT ${${EXTERNAL}_UPPERCASE}_ROOT_DIR)
    string(TOLOWER ${EXTERNAL} ${EXTERNAL}_LOWERCASE)
    set(${${EXTERNAL}_UPPERCASE}_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/${${EXTERNAL}_LOWERCASE}")
  endif ()
endforeach()

find_package(Qt5LinguistTools REQUIRED)
find_package(Qt5LinguistToolsMacros)

if (WIN32)
  set(CMAKE_CXX_FLAGS_DEBUG  "${CMAKE_CXX_FLAGS_DEBUG} -bigobj")
  add_definitions(-D_USE_MATH_DEFINES) # apparently needed to get M_PI and other defines from cmath/math.h
  add_definitions(-DWINDOWS_LEAN_AND_MEAN) # needed to make sure windows doesn't go to crazy with its defines
endif()

# grab the implementation and header files from src dirs
file(GLOB_RECURSE INTERFACE_SRCS "src/*.cpp" "src/*.h")
GroupSources("src")
list(APPEND INTERFACE_SRCS ${RESOURCES_RCC})

# grab the Objective-C sources on OS X
if (APPLE)
  file(GLOB_RECURSE INTERFACE_OBJCPP_SRCS "src/*.m" "src/*.mm")
  list(APPEND INTERFACE_SRCS ${INTERFACE_OBJCPP_SRCS})
endif ()

# Add SpeechRecognizer if on Windows or OS X, otherwise remove
if (WIN32)
  # Use .cpp and .h files as is.
elseif (APPLE)
  get_filename_component(SPEECHRECOGNIZER_CPP "src/SpeechRecognizer.cpp" ABSOLUTE)
  list(REMOVE_ITEM INTERFACE_SRCS ${SPEECHRECOGNIZER_CPP})
else ()
  get_filename_component(SPEECHRECOGNIZER_H "src/SpeechRecognizer.h" ABSOLUTE)
  list(REMOVE_ITEM INTERFACE_SRCS ${SPEECHRECOGNIZER_H})
  get_filename_component(SPEECHRECOGNIZER_CPP "src/SpeechRecognizer.cpp" ABSOLUTE)
  list(REMOVE_ITEM INTERFACE_SRCS ${SPEECHRECOGNIZER_CPP})
endif ()

find_package(
  Qt5 COMPONENTS
  Gui Widgets Multimedia Network Qml Quick Svg
  ${PLATFORM_QT_COMPONENTS}
  WebChannel WebSockets
)

# grab the ui files in resources/ui
file (GLOB_RECURSE QT_UI_FILES ui/*.ui)
source_group("UI Files" FILES ${QT_UI_FILES})
# have qt5 wrap them and generate the appropriate header files
qt5_wrap_ui(QT_UI_HEADERS "${QT_UI_FILES}")

# add them to the interface source files
set(INTERFACE_SRCS ${INTERFACE_SRCS} "${QT_UI_HEADERS}" "${QT_RESOURCES}")

# translation disabled until we strip out the line numbers
# set(QM ${TARGET_NAME}_en.qm)
# set(TS ${TARGET_NAME}_en.ts)
# qt5_create_translation_custom(${QM} ${INTERFACE_SRCS} ${QT_UI_FILES} ${TS})

# setup the android parameters that will help us produce an APK
if (ANDROID)
  set(ANDROID_SDK_ROOT $ENV{ANDROID_HOME})
  set(BUILD_SHARED_LIBS ON)
endif ()

if (APPLE)

  # configure CMake to use a custom Info.plist
  set_target_properties(${this_target} PROPERTIES MACOSX_BUNDLE_INFO_PLIST MacOSXBundleInfo.plist.in)

  set(MACOSX_BUNDLE_BUNDLE_NAME "Vircadia")
  if (PRODUCTION_BUILD)
    set(MACOSX_BUNDLE_GUI_IDENTIFIER com.vircadia.interface)
  else ()
    if (DEV_BUILD)
      set(MACOSX_BUNDLE_GUI_IDENTIFIER com.vircadia.interface-dev)
    elseif (PR_BUILD)
      set(MACOSX_BUNDLE_GUI_IDENTIFIER com.vircadia.interface-pr)
    endif ()
  endif ()

  # set how the icon shows up in the Info.plist file
  set(MACOSX_BUNDLE_ICON_FILE "${INTERFACE_ICON_FILENAME}")

  # set where in the bundle to put the resources file
  set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/icon/${INTERFACE_ICON_FILENAME} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

  # append the discovered resources to our list of interface sources
  list(APPEND INTERFACE_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/icon/${INTERFACE_ICON_FILENAME})
endif()

if (UNIX)
  set(BUILD_SHARED_LIBS ON)
endif ()

# create the executable, make it a bundle on OS X
if (APPLE)
  add_executable(${TARGET_NAME} MACOSX_BUNDLE ${INTERFACE_SRCS} ${QM})

  # make sure the output name for the .app bundle is correct
  # Fix up the rpath so macdeployqt works
  set_target_properties(${TARGET_NAME} PROPERTIES INSTALL_RPATH "@executable_path/../Frameworks")
elseif (WIN32)
  # configure an rc file for the chosen icon
  set(CONFIGURE_ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/icon/${INTERFACE_ICON_FILENAME}")
  set(CONFIGURE_ICON_RC_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/Icon.rc")
  configure_file("${HF_CMAKE_DIR}/templates/Icon.rc.in" ${CONFIGURE_ICON_RC_OUTPUT})

  set(APP_FULL_NAME "Vircadia")
  set(CONFIGURE_VERSION_INFO_RC_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/VersionInfo.rc")
  configure_file("${HF_CMAKE_DIR}/templates/VersionInfo.rc.in" ${CONFIGURE_VERSION_INFO_RC_OUTPUT})

  # add an executable that also has the icon itself and the configured rc file as resources
  add_executable(${TARGET_NAME} WIN32 ${INTERFACE_SRCS} ${QM} ${CONFIGURE_ICON_RC_OUTPUT} ${CONFIGURE_VERSION_INFO_RC_OUTPUT})
  ##^^^^^ creates native Win32 app w/o cmd console vvvvvv forces cmd console for logging
  # add_executable(${TARGET_NAME} ${INTERFACE_SRCS} ${QM} ${CONFIGURE_ICON_RC_OUTPUT} ${CONFIGURE_VERSION_INFO_RC_OUTPUT})

  if (NOT DEV_BUILD)
    add_custom_command(
      TARGET ${TARGET_NAME}
      POST_BUILD
      COMMAND "mt.exe" -manifest "${CMAKE_CURRENT_SOURCE_DIR}/interface.exe.manifest" -inputresource:"$<TARGET_FILE:${TARGET_NAME}>"\;\#1 -outputresource:"$<TARGET_FILE:${TARGET_NAME}>"\;\#1
      COMMENT "Adding OS version support manifest to exe"
    )
  endif()

elseif (ANDROID)
  # on android the Interface target is a library that gets linked/used by the APK shell that qtcreateapk produces
  add_library(${TARGET_NAME} ${INTERFACE_SRCS} ${QM})
else ()
  add_executable(${TARGET_NAME} ${INTERFACE_SRCS} ${QM})
endif ()


if (BUILD_TOOLS AND NPM_EXECUTABLE)
  # require JSDoc to be build before interface is deployed
  add_dependencies(resources jsdoc)
endif()

if (WIN32 OR APPLE)
  add_dependencies(${TARGET_NAME} resources)
endif()

if (SCREENSHARE)
  add_dependencies(${TARGET_NAME} screenshare)
endif()

if (WIN32)
  # These are external plugins, but we need to do the 'add dependency' here so that their
  # binary directories get added to the fixup path

  if (USE_SIXENSE)
    add_dependency_external_projects(sixense)
  endif ()

  add_dependency_external_projects(neuron)
  add_dependency_external_projects(wasapi)
  add_dependency_external_projects(steamworks)
endif()

# disable /OPT:REF and /OPT:ICF for the Debug builds
# This will prevent the following linker warnings
# LINK : warning LNK4075: ignoring '/INCREMENTAL' due to '/OPT:ICF' specification
if (WIN32)
  set_property(TARGET ${TARGET_NAME} APPEND_STRING PROPERTY LINK_FLAGS_DEBUG "/OPT:NOREF /OPT:NOICF")
endif()

# link required hifi libraries
link_hifi_libraries(
  shared workload task octree ktx gpu gl procedural graphics graphics-scripting render
  pointers recording hfm model-serializers networking material-networking
  model-networking model-baker entities avatars
  audio audio-client animation physics
  render-utils entities-renderer avatars-renderer ui qml auto-updater midi
  controllers plugins image platform
  ui-plugins display-plugins input-plugins
  # Platform specific GL libraries
  ${PLATFORM_GL_BACKEND}
  # Plaform specific input & display plugin libraries
  ${PLATFORM_PLUGIN_LIBRARIES}
  shaders
)
include_hifi_library_headers(script-engine)

# include the binary directory of render-utils for shader includes
target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_BINARY_DIR}/libraries/render-utils")

# include OpenSSL
target_openssl()

target_bullet()

set(OpenGL_GL_PREFERENCE "LEGACY")
target_opengl()
add_crashpad()
target_breakpad()
target_json()

# perform standard include and linking for found externals
foreach(EXTERNAL ${OPTIONAL_EXTERNALS})

  if (${${EXTERNAL}_UPPERCASE}_REQUIRED)
    find_package(${EXTERNAL} REQUIRED)
  else ()
    find_package(${EXTERNAL})
  endif ()

  if (${${EXTERNAL}_UPPERCASE}_FOUND AND NOT DISABLE_${${EXTERNAL}_UPPERCASE})
    add_definitions(-DHAVE_${${EXTERNAL}_UPPERCASE})

    # include the library directories (ignoring warnings)
    if (NOT ${${EXTERNAL}_UPPERCASE}_INCLUDE_DIRS)
      set(${${EXTERNAL}_UPPERCASE}_INCLUDE_DIRS ${${${EXTERNAL}_UPPERCASE}_INCLUDE_DIR})
    endif ()

    include_directories(SYSTEM ${${${EXTERNAL}_UPPERCASE}_INCLUDE_DIRS})

    # perform the system include hack for OS X to ignore warnings
    if (APPLE)
      foreach(EXTERNAL_INCLUDE_DIR  ${${${EXTERNAL}_UPPERCASE}_INCLUDE_DIRS})
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isystem ${EXTERNAL_INCLUDE_DIR}")
      endforeach()
    endif ()

    if (NOT ${${EXTERNAL}_UPPERCASE}_LIBRARIES)
      set(${${EXTERNAL}_UPPERCASE}_LIBRARIES ${${${EXTERNAL}_UPPERCASE}_LIBRARY})
    endif ()

    if (NOT APPLE OR NOT ${${EXTERNAL}_UPPERCASE} MATCHES "SIXENSE")
      target_link_libraries(${TARGET_NAME} ${${${EXTERNAL}_UPPERCASE}_LIBRARIES})
    elseif (APPLE AND NOT INSTALLER_BUILD)
      add_definitions(-DSIXENSE_LIB_FILENAME=\"${${${EXTERNAL}_UPPERCASE}_LIBRARY_RELEASE}\")
    endif ()
  endif ()
endforeach()

# include headers for interface and InterfaceConfig.
target_include_directories(${TARGET_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/src")

if (ANDROID)
  find_library(ANDROID_LOG_LIB log)
  target_link_libraries(${TARGET_NAME} ${ANDROID_LOG_LIB})
endif ()

target_link_libraries(
  ${TARGET_NAME}
  Qt5::Gui Qt5::Network Qt5::Multimedia Qt5::Widgets
  Qt5::Qml Qt5::Quick Qt5::Svg
  Qt5::WebChannel
  ${PLATFORM_QT_LIBRARIES}
)

if (UNIX AND NOT ANDROID AND NOT VIRCADIA_THREAD_DEBUGGING)
    if (CMAKE_SYSTEM_NAME MATCHES "Linux")
        # Linux
        target_link_libraries(${TARGET_NAME} pthread atomic)
    else ()
        # OSX
        target_link_libraries(${TARGET_NAME} pthread)
    endif ()
endif()

# assume we are using a Qt build without bearer management
add_definitions(-DQT_NO_BEARERMANAGEMENT)

if (APPLE)
  # link in required OS X frameworks and include the right GL headers
  find_library(OpenGL OpenGL)
  find_library(AppKit AppKit)

  target_link_libraries(${TARGET_NAME} ${OpenGL} ${AppKit})

  # setup install of OS X interface bundle
  install(TARGETS ${TARGET_NAME}
    BUNDLE DESTINATION ${INTERFACE_INSTALL_DIR}
    COMPONENT ${CLIENT_COMPONENT}
  )

  set(SCRIPTS_INSTALL_DIR "${INTERFACE_INSTALL_APP_PATH}/Contents/Resources")
  set(RESOURCES_DEV_DIR "$<TARGET_FILE_DIR:${TARGET_NAME}>/../Resources")

  add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    # copy script files beside the executable
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
      "${CMAKE_SOURCE_DIR}/scripts"
      "${RESOURCES_DEV_DIR}/scripts"
    # copy JSDoc files beside the executable
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
      "${CMAKE_SOURCE_DIR}/tools/jsdoc/out"
      "${RESOURCES_DEV_DIR}/jsdoc"
    # copy the resources files beside the executable
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${RESOURCES_RCC}"
      "${RESOURCES_DEV_DIR}"
    # FIXME, the edit script code loads HTML from the scripts folder
    # which in turn relies on CSS that refers to the fonts.  In theory
    # we should be able to modify the CSS to reference the QRC path to
    # the ttf files, but doing so generates a CORS policy violation,
    # so we have to retain a copy of the fonts outside of the resources binary
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
      "${PROJECT_SOURCE_DIR}/resources/fonts"
      "${RESOURCES_DEV_DIR}/fonts"
    #copy serverless for android
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
      "${PROJECT_SOURCE_DIR}/resources/serverless"
      "${RESOURCES_DEV_DIR}/serverless"
    # add redirect json to macOS builds.
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${PROJECT_SOURCE_DIR}/resources/serverless/redirect.json"
      "${RESOURCES_DEV_DIR}/serverless/redirect.json"
    )

  if (SCREENSHARE)
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
      # copy screenshare app to the resource folder
      COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_BINARY_DIR}/../screenshare/hifi-screenshare-darwin-x64/hifi-screenshare.app"
        "${RESOURCES_DEV_DIR}/hifi-screenshare.app"
      )
  endif()

  # call the fixup_interface macro to add required bundling commands for installation
  fixup_interface()

else()
  set(INTERFACE_EXEC_DIR "$<TARGET_FILE_DIR:${TARGET_NAME}>")
  set(RESOURCES_DEV_DIR "${INTERFACE_EXEC_DIR}/resources")

  # copy the resources files beside the executable
  add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${RESOURCES_RCC}"
      "${INTERFACE_EXEC_DIR}"
    # FIXME, the edit script code loads HTML from the scripts folder
    # which in turn relies on CSS that refers to the fonts.  In theory
    # we should be able to modify the CSS to reference the QRC path to
    # the ttf files, but doing so generates a CORS policy violation,
    # so we have to retain a copy of the fonts outside of the resources binary
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
      "${PROJECT_SOURCE_DIR}/resources/fonts"
      "${RESOURCES_DEV_DIR}/fonts"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
      "${CMAKE_SOURCE_DIR}/scripts"
      "${INTERFACE_EXEC_DIR}/scripts"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${PROJECT_SOURCE_DIR}/resources/serverless/tutorial.json"
      "${RESOURCES_DEV_DIR}/serverless/tutorial.json"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${PROJECT_SOURCE_DIR}/resources/serverless/redirect.json"
      "${RESOURCES_DEV_DIR}/serverless/redirect.json"
    # copy JSDoc files beside the executable
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
      "${CMAKE_SOURCE_DIR}/tools/jsdoc/out"
      "${INTERFACE_EXEC_DIR}/jsdoc"
  )

  # link target to external libraries
  if (WIN32)
    target_link_libraries(${TARGET_NAME} wsock32.lib Winmm.lib)

    # setup install of executable and things copied by fixup/windeployqt
    install(
      DIRECTORY "$<TARGET_FILE_DIR:${TARGET_NAME}>/"
      DESTINATION ${INTERFACE_INSTALL_DIR}
      COMPONENT ${CLIENT_COMPONENT}
      PATTERN "*.pdb" EXCLUDE
      PATTERN "*.lib" EXCLUDE
      PATTERN "*.exp" EXCLUDE
    )

    set(SCRIPTS_INSTALL_DIR "${INTERFACE_INSTALL_DIR}")

    set(EXECUTABLE_COMPONENT ${CLIENT_COMPONENT})

    optional_win_executable_signing()
  endif()
endif()

if (DEV_BUILD AND (APPLE OR UNIX))
  # create a qt.conf file to override hard-coded search paths in Qt libs
  set(QT_LIB_PATH "${QT_CMAKE_PREFIX_PATH}/../..")
  if (APPLE)
    set(QT_CONF_FILE "${RESOURCES_DEV_DIR}/../Resources/qt.conf")
  else ()
    set(QT_CONF_FILE "${INTERFACE_EXEC_DIR}/qt.conf")
  endif ()
  file(GENERATE
    OUTPUT "${QT_CONF_FILE}"
    CONTENT "[Paths]\nPrefix=${QT_LIB_PATH}\n"
  )
endif()

if (SCRIPTS_INSTALL_DIR)
  # setup install of scripts beside interface executable
  install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/scripts/"
    DESTINATION ${SCRIPTS_INSTALL_DIR}/scripts
    COMPONENT ${CLIENT_COMPONENT}
  )
endif()

if (DOWNLOAD_SERVERLESS_CONTENT)
  add_dependency_external_projects(serverless-content)

  ExternalProject_Get_Property(serverless-content SOURCE_DIR)

  # for dev builds, copy the serverless content to the resources folder
  add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
      "${SOURCE_DIR}"
      "${RESOURCES_DEV_DIR}/serverless"
  )
endif ()

if (WIN32)
    set(EXTRA_DEPLOY_OPTIONS "--qmldir  \"${PROJECT_SOURCE_DIR}/resources/qml\"")

    set(TARGET_INSTALL_DIR ${INTERFACE_INSTALL_DIR})
    set(TARGET_INSTALL_COMPONENT ${CLIENT_COMPONENT})
    manually_install_openssl_for_qt()

    package_libraries_for_deployment()
endif()

add_dependency_external_projects(GifCreator)
find_package(GifCreator REQUIRED)
target_include_directories(${TARGET_NAME} PUBLIC ${GIFCREATOR_INCLUDE_DIRS})

# tell CMake to exclude ui_console.h for policy CMP0071
set_property(SOURCE ui_console.h PROPERTY SKIP_AUTOMOC ON)
