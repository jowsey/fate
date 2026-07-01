cmake_minimum_required(VERSION 3.28...4.3)
project(fate VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

include(FetchContent)

# Vulkan SDK
if (NOT DEFINED ENV{VULKAN_SDK})
    message(FATAL_ERROR "VULKAN_SDK environment variable not set")
else ()
    message("Found Vulkan SDK at $ENV{VULKAN_SDK}")
endif ()

find_package(Vulkan REQUIRED)
include_directories(SYSTEM "$ENV{VULKAN_SDK}/include")

find_program(SLANGC_EXE slangc HINTS "$ENV{VULKAN_SDK}/bin" REQUIRED)

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
        src/IndexAllocator.cpp
        src/IndexAllocator.h
)

target_compile_definitions(fate PRIVATE
        FATE_VERSION="${fate_VERSION}"
        FATE_VERSION_MAJOR=${fate_VERSION_MAJOR}
        FATE_VERSION_MINOR=${fate_VERSION_MINOR}
        FATE_VERSION_PATCH=${fate_VERSION_PATCH}
)

# Windows app icon
if (WIN32)
    target_sources(fate PRIVATE "resources-build/app_icon.rc")
endif ()

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

# Copy resources to build directory
file(GLOB_RECURSE RESOURCE_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/*")
set(RESOURCE_TIMESTAMP "${CMAKE_CURRENT_BINARY_DIR}/resources.stamp")

add_custom_command(
        OUTPUT "${RESOURCE_TIMESTAMP}"
        DEPENDS ${RESOURCE_FILES}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/dist/resources"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/resources" "${CMAKE_CURRENT_BINARY_DIR}/dist/resources"
        COMMAND ${CMAKE_COMMAND} -E touch "${RESOURCE_TIMESTAMP}"
        COMMENT "Copying resources to build directory"
)

add_custom_target(copy_resources DEPENDS "${RESOURCE_TIMESTAMP}")

# Compile shaders
file(GLOB_RECURSE SHADER_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.slang")

set(SPIRV_OUTPUTS "")
foreach (SHADER_FILE IN LISTS SHADER_FILES)
    get_filename_component(SHADER_NAME "${SHADER_FILE}" NAME)
    set(SPIRV_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/dist/resources/Shaders/${SHADER_NAME}.spv")
    add_custom_command(
            OUTPUT "${SPIRV_OUTPUT}"
            #            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/dist/resources/Shaders"
            COMMAND ${SLANGC_EXE} "${SHADER_FILE}" -target spirv -o "${SPIRV_OUTPUT}"
            DEPENDS "${SHADER_FILE}" "${RESOURCE_TIMESTAMP}"
            COMMENT "Compiling shader ${SHADER_NAME} to SPIR-V"
    )
    list(APPEND SPIRV_OUTPUTS "${SPIRV_OUTPUT}")
endforeach ()

add_custom_target(compile_shaders ALL DEPENDS ${SPIRV_OUTPUTS})
add_dependencies(compile_shaders copy_resources)

add_dependencies(fate compile_shaders)

# Volk
FetchContent_Declare(
        volk
        GIT_REPOSITORY https://github.com/zeux/volk.git
        GIT_TAG 1.4.350
        GIT_SHALLOW TRUE
)

add_compile_definitions(VK_NO_PROTOTYPES)

FetchContent_MakeAvailable(volk)
target_link_libraries(fate PRIVATE volk)

# VMA
FetchContent_Declare(
        vma
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG v3.4.0
        GIT_SHALLOW TRUE
        SYSTEM
)

FetchContent_MakeAvailable(vma)
target_link_libraries(fate PRIVATE GPUOpen::VulkanMemoryAllocator)

# KTX
#FetchContent_Declare(
#        ktx
#        GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
#        GIT_TAG v4.4.2
#        GIT_SHALLOW TRUE
#)
#
#set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)
#set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
#set(KTX_FEATURE_APPS OFF CACHE BOOL "" FORCE)
#
#FetchContent_MakeAvailable(ktx)
#target_link_libraries(fate PRIVATE ktx)

# SDL3
FetchContent_Declare(
        sdl3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.4.10
        GIT_SHALLOW TRUE
)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_DOCS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(sdl3)
target_link_libraries(fate PRIVATE SDL3::SDL3)

# GLM
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.3
        GIT_SHALLOW TRUE
)

target_compile_definitions(fate PRIVATE GLM_FORCE_DEPTH_ZERO_TO_ONE)

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
        #        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        #        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)

target_compile_definitions(imgui PUBLIC
        IMGUI_DISABLE_DEFAULT_FONT
        IMGUI_IMPL_VULKAN_USE_VOLK
)

target_link_libraries(imgui PRIVATE Vulkan::Vulkan SDL3::SDL3 volk)
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