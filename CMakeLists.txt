cmake_minimum_required(VERSION 3.31)
project(velvet VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

add_executable(velvet main.cpp)

target_compile_definitions(velvet PRIVATE VELVET_VERSION="${PROJECT_VERSION}")

# Static linking for Windows MINGW
if (WIN32 AND MINGW)
    target_link_options(velvet PRIVATE "-static-libgcc" "-static-libstdc++" "-static")
endif ()

set_target_properties(velvet PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/dist/bin"
)

add_custom_command(TARGET velvet POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/dist/Resources"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Resources" "${CMAKE_CURRENT_BINARY_DIR}/dist/Resources"
)

# CLI11
include(FetchContent)

FetchContent_Declare(
        CLI11
        QUIET
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG v2.6.2
)

FetchContent_MakeAvailable(CLI11)

target_link_libraries(velvet PRIVATE CLI11::CLI11)

