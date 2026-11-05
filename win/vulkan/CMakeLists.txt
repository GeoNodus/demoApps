cmake_minimum_required(VERSION 3.16)

# GeoNodus demo on WinAPI + Vulkan 1.0: vulkan-1.dll is loaded at run time, the Khronos headers are in
# third_party/vulkan, the shaders' SPIR-V is in shaders/shaders_spv.h (rebuild with shaders/compile_shaders.bat). No
# Vulkan SDK is needed. geonodus comes from ../lib, the Release build of the library for the compiler's architecture
# (x64 or arm64)
project(geonodus_vulkan LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

string(TOLOWER "${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}" GN_ARCH)
set(GN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../lib")
set(COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../common")
# Demo scenes built in code, shared by the apps of every OS (one function per node type)
set(SCENES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../scenes")
file(GLOB SCENES_SOURCES CONFIGURE_DEPENDS "${SCENES_DIR}/*.cpp" "${SCENES_DIR}/*.h")

add_executable(geonodus_vulkan WIN32
    main.cpp
    vk_renderer.cpp
    vk_renderer.h
    shaders/shaders_spv.h
    ${COMMON_DIR}/demo_app.cpp
    ${COMMON_DIR}/demo_app.h
    ${COMMON_DIR}/demo_math.h
    ${COMMON_DIR}/gn_scene.cpp
    ${COMMON_DIR}/gn_scene.h
    ${SCENES_SOURCES}
)
target_include_directories(geonodus_vulkan PRIVATE "${GN_DIR}/include" "${COMMON_DIR}" "${SCENES_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
target_compile_definitions(geonodus_vulkan PRIVATE WIN32 UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
    _WIN32_WINNT=0x0A00)
target_link_libraries(geonodus_vulkan PRIVATE "${GN_DIR}/${GN_ARCH}/geonodus.lib" comctl32 comdlg32 shell32)
if(MSVC)
    target_compile_options(geonodus_vulkan PRIVATE /W4 /EHsc /utf-8)
endif()

# geonodus.dll — next to the exe
add_custom_command(TARGET geonodus_vulkan POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${GN_DIR}/${GN_ARCH}/geonodus.dll"
            $<TARGET_FILE_DIR:geonodus_vulkan>)
