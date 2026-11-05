cmake_minimum_required(VERSION 3.20)

# GeoNodus demo on AppKit + Metal (Swift): the only dependency besides the system frameworks is geonodus (../lib, the
# Release build of the demo edition of the library, arm64). Swift needs the Xcode or Ninja generator:
#   cmake -S demoApps/mac/metal -B <build> -G Xcode && cmake --build <build> --config Release
#   cmake -S demoApps/mac/metal -B <build> -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build <build>
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "macOS architecture")

project(geonodus_metal LANGUAGES CXX Swift)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Swift 5 language mode (AppKit without the strict concurrency checks of Swift 6). Xcode takes it as a build setting:
# -swift-version in the flags would override its language mode with a warning
if(NOT CMAKE_GENERATOR STREQUAL "Xcode")
    set(CMAKE_Swift_LANGUAGE_VERSION 5)
endif()

set(GN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../lib")
# Demo scenes built in code, shared by the apps of every OS (one function per node type)
set(SCENES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../scenes")
file(GLOB SCENES_SOURCES CONFIGURE_DEPENDS "${SCENES_DIR}/*.cpp" "${SCENES_DIR}/*.h")

# The scenes (C++) and their C interface for Swift
add_library(demo_scenes STATIC ${SCENES_SOURCES} bridge/demo_scenes_c.cpp bridge/demo_scenes_c.h)
target_include_directories(demo_scenes PRIVATE "${GN_DIR}/include" "${SCENES_DIR}")

file(GLOB APP_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Sources/*.swift")
add_executable(geonodus_metal MACOSX_BUNDLE ${APP_SOURCES})
set_target_properties(geonodus_metal PROPERTIES
    OUTPUT_NAME "GeoNodus Metal Demo"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    MACOSX_BUNDLE_BUNDLE_NAME "GeoNodus Metal Demo"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.geonodus.demo.metal"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    MACOSX_BUNDLE_BUNDLE_VERSION "1"
    # libgeonodus.dylib (install name @rpath/libgeonodus.dylib) — in Contents/Frameworks of the bundle
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "@executable_path/../Frameworks"
    XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@executable_path/../Frameworks"
    XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/BridgingHeader.h"
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.geonodus.demo.metal"
    XCODE_ATTRIBUTE_SWIFT_VERSION "5.0"
    "XCODE_ATTRIBUTE_SWIFT_OPTIMIZATION_LEVEL[variant=Debug]" "-Onone"
    "XCODE_ATTRIBUTE_SWIFT_OPTIMIZATION_LEVEL[variant=Release]" "-O"
)
# C headers for Swift: geonodus.h and the scenes' C interface through the bridging header (Xcode — the attribute above)
target_compile_options(geonodus_metal PRIVATE
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${GN_DIR}/include>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${CMAKE_CURRENT_SOURCE_DIR}>")
if(NOT CMAKE_GENERATOR STREQUAL "Xcode")
    target_compile_options(geonodus_metal PRIVATE
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-import-objc-header ${CMAKE_CURRENT_SOURCE_DIR}/BridgingHeader.h>")
endif()
target_link_libraries(geonodus_metal PRIVATE demo_scenes "${GN_DIR}/libgeonodus.dylib" c++
    "-framework AppKit" "-framework Metal" "-framework MetalKit" "-framework QuartzCore")

# libgeonodus.dylib — into the bundle, then the bundle is signed again (ad hoc) with it
add_custom_command(TARGET geonodus_metal POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:geonodus_metal>/Frameworks"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${GN_DIR}/libgeonodus.dylib"
            "$<TARGET_BUNDLE_CONTENT_DIR:geonodus_metal>/Frameworks/"
    COMMAND codesign --force --sign - "$<TARGET_BUNDLE_CONTENT_DIR:geonodus_metal>/Frameworks/libgeonodus.dylib"
    COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:geonodus_metal>"
)
