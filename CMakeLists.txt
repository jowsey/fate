cmake_minimum_required(VERSION 3.31)
project(fate VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

include(FetchContent)

add_executable(fate
        src/main.cpp
        src/FateRenderer.cpp
        src/FateRenderer.h
)

target_compile_definitions(fate PRIVATE FATE_VERSION="${PROJECT_VERSION}")

# Static linking for Windows MINGW
if (WIN32 AND MINGW)
    target_link_options(fate PRIVATE "-static-libgcc" "-static-libstdc++" "-static")
endif ()

# Build directory
set_target_properties(fate PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/dist/bin"
)

add_custom_command(TARGET fate POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/dist/Resources"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Resources" "${CMAKE_CURRENT_BINARY_DIR}/dist/Resources"
)

# glad
add_library(glad STATIC
        vendor/glad/src/glad.c
)

target_include_directories(glad PUBLIC
        vendor/glad/include
)

target_link_libraries(fate PRIVATE glad)

# GLFW
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)
target_link_libraries(fate PRIVATE glfw)

# GLM
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.3
)

FetchContent_MakeAvailable(glm)
target_link_libraries(fate PRIVATE glm::glm)

# CLI11
FetchContent_Declare(
        CLI11
        QUIET
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG v2.6.2
)

FetchContent_MakeAvailable(CLI11)
target_link_libraries(fate PRIVATE CLI11::CLI11)
