cmake_minimum_required(VERSION 3.31)
project(CsharpHosting)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

add_executable(CsharpHosting main.cpp)

# CLI11
include(FetchContent)

FetchContent_Declare(
        CLI11
        QUIET
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG v2.6.2
)

FetchContent_MakeAvailable(CLI11)

target_link_libraries(CsharpHosting PRIVATE CLI11::CLI11)

# Static linking for Windows MINGW
if (WIN32 AND MINGW)
    target_link_options(CsharpHosting PRIVATE "-static-libgcc" "-static-libstdc++" "-static")
endif ()