cmake_minimum_required(VERSION 3.28...4.3)
project(fate VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

include(FetchContent)

add_executable(fate
        src/utils/Paths.cpp
        src/utils/Paths.h
        src/utils/Files.cpp
        src/utils/Files.h
        src/FateEngine.cpp
        src/FateEngine.h
        src/FateRenderer.cpp
        src/FateRenderer.h
        src/GPUMeshHandle.h
        src/main.cpp
        src/Material.h
        src/Mesh.h
        src/Scene.cpp
        src/Scene.h
        src/SceneObject.cpp
        src/SceneObject.h
        src/SceneTransform.cpp
        src/SceneTransform.h
        src/TextureData.h
        src/Vertex.h
)

target_compile_definitions(fate PRIVATE FATE_VERSION="${PROJECT_VERSION}")

# Static linking
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
target_link_options(fate PRIVATE "-static-libgcc" "-static-libstdc++" "-static")

# Release build optimizations
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    # general optimizations
    target_compile_options(fate PRIVATE -mavx2 -ffast-math)

    # dead code elimination
    target_compile_options(fate PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(fate PRIVATE -Wl,--gc-sections)

    # glm SIMD support
    target_compile_definitions(fate PRIVATE
            GLM_FORCE_INTRINSICS
            GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
    )

    # link-time optimization
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipoSupported OUTPUT error)
    if (ipoSupported)
        message(STATUS "LTO enabled")
        set_target_properties(fate PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif ()
endif ()

# Build directory
set_target_properties(fate PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/dist/bin"
)

add_custom_command(TARGET fate POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/dist/resources"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/resources" "${CMAKE_CURRENT_BINARY_DIR}/dist/resources"
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
        GIT_SHALLOW TRUE
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
        GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(glm)
target_link_libraries(fate PRIVATE glm::glm)

# ImGui
FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.92.8-docking
        GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(imgui)

set(IMGUI_SOURCES
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)

target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_DEFAULT_FONT)

target_link_libraries(imgui PRIVATE glfw)
target_link_libraries(fate PRIVATE imgui)

# assimp
FetchContent_Declare(
        assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG v6.0.5
        GIT_SHALLOW TRUE
)

set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)

set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)

set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(assimp)
target_link_libraries(fate PRIVATE assimp)

# wuffs
FetchContent_Declare(
        wuffs
        GIT_REPOSITORY https://github.com/google/wuffs.git
        GIT_TAG v0.3.4
        GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(wuffs)
add_library(wuffs INTERFACE)
target_include_directories(wuffs INTERFACE ${wuffs_SOURCE_DIR}/release/c)
target_link_libraries(fate PRIVATE wuffs)

# CLI11
FetchContent_Declare(
        CLI11
        QUIET
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG v2.6.2
        GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(CLI11)
target_link_libraries(fate PRIVATE CLI11::CLI11)