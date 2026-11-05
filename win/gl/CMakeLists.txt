cmake_minimum_required(VERSION 3.16)

# GeoNodus demo on WinAPI + OpenGL 3.3: the only dependencies are opengl32 and geonodus (../lib, the Release build of
# the library for the compiler's architecture — x64 or arm64)
project(geonodus_gl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

string(TOLOWER "${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}" GN_ARCH)
set(GN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../lib")
set(COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../common")
# Demo scenes built in code, shared by the apps of every OS (one function per node type)
set(SCENES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../scenes")
file(GLOB SCENES_SOURCES CONFIGURE_DEPENDS "${SCENES_DIR}/*.cpp" "${SCENES_DIR}/*.h")

add_executable(geonodus_gl WIN32
    main.cpp
    gl_renderer.cpp
    gl_renderer.h
    ${COMMON_DIR}/demo_app.cpp
    ${COMMON_DIR}/demo_app.h
    ${COMMON_DIR}/demo_math.h
    ${COMMON_DIR}/gn_scene.cpp
    ${COMMON_DIR}/gn_scene.h
    ${SCENES_SOURCES}
)
target_include_directories(geonodus_gl PRIVATE "${GN_DIR}/include" "${COMMON_DIR}" "${SCENES_DIR}")
target_compile_definitions(geonodus_gl PRIVATE WIN32 UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
target_link_libraries(geonodus_gl PRIVATE "${GN_DIR}/${GN_ARCH}/geonodus.lib" opengl32 comctl32 comdlg32 shell32)
if(MSVC)
    target_compile_options(geonodus_gl PRIVATE /W4 /EHsc /utf-8)
endif()

# geonodus.dll — next to the exe
add_custom_command(TARGET geonodus_gl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${GN_DIR}/${GN_ARCH}/geonodus.dll" $<TARGET_FILE_DIR:geonodus_gl>)
