cmake_minimum_required(VERSION 3.28...4.4)
project(fate VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# Release build optimizations
# avx2
if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    if (MSVC)
        add_compile_options("$<$<CONFIG:Release>:/arch:AVX2>")
    else ()
        add_compile_options("$<$<CONFIG:Release>:-march=x86-64-v3>")
    endif ()
endif ()

# fast math
if (MSVC)
    add_compile_options("$<$<CONFIG:Release>:/fp:contract>")
else ()
    add_compile_options("$<$<CONFIG:Release>:-ffp-contract=fast;-fno-math-errno>")
endif ()

# dead code elimination
if (MSVC)
    add_compile_options("$<$<CONFIG:Release>:/Gy>")
    add_link_options("$<$<CONFIG:Release>:/OPT:REF>")
elseif (APPLE)
    add_link_options("$<$<CONFIG:Release>:-Wl,-dead_strip>")
else ()
    add_compile_options("$<$<CONFIG:Release>:-ffunction-sections;-fdata-sections>")
    add_link_options("$<$<CONFIG:Release>:-Wl,--gc-sections>")
endif ()

# Static linking
set(BUILD_SHARED_LIBS OFF)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT APPLE)
    if (MINGW)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
    else ()
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    endif ()
endif ()

add_executable(fate
        src/utils/Paths.cpp
        src/utils/Paths.h
        src/utils/Files.cpp
        src/utils/Files.h
        src/Engine.cpp
        src/Engine.h
        src/Editor.cpp
        src/Editor.h
        src/EditorUI.cpp
        src/EditorUI.h
        src/Renderer.cpp
        src/Renderer.h
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
        src/CubemapData.h
)

# link-time optimization
include(CheckIPOSupported)
check_ipo_supported(RESULT ipoSupported OUTPUT error)
if (ipoSupported)
    message(STATUS "LTO available!")
    set_target_properties(fate PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
else ()
    message(STATUS "LTO unavailable: ${error}")
endif ()

target_compile_definitions(fate PRIVATE
        FATE_VERSION="${fate_VERSION}"
        FATE_VERSION_MAJOR=${fate_VERSION_MAJOR}
        FATE_VERSION_MINOR=${fate_VERSION_MINOR}
        FATE_VERSION_PATCH=${fate_VERSION_PATCH}
)

# Strict warnings
set_target_properties(fate PROPERTIES COMPILE_WARNING_AS_ERROR ON)

# -Wmissing-designated-field-initializers very annoying for Vulkan structs
if (MSVC)
    target_compile_options(fate PRIVATE /W4)
else ()
    target_compile_options(fate PRIVATE -Wall -Wextra -pedantic -Wno-missing-field-initializers)
endif ()

# Windows app icon
if (WIN32)
    target_sources(fate PRIVATE "resources-build/app_icon.rc")
endif ()

# Build directory
set_target_properties(fate PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/dist/bin$<1:>"
        OUTPUT_NAME "fate-editor"
)

# Copy resources to build directory
add_custom_target(copy_resources
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${CMAKE_CURRENT_SOURCE_DIR}/resources" "${CMAKE_CURRENT_BINARY_DIR}/dist/resources"
        COMMENT "Copying resources to build directory"
)

# Vulkan SDK
find_package(Vulkan REQUIRED)
find_program(SLANGC_EXE slangc HINTS ENV VULKAN_SDK PATH_SUFFIXES bin REQUIRED)

# Compile shaders
# todo it's kinda yucky that we depend on it being non-recursive to move stuff that doesn't export symbols to lib/
#      find a way to make it recursive but not complain about libs not exporting symbols
file(GLOB SHADER_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.slang")

set(SHADERS_DIR "${CMAKE_CURRENT_BINARY_DIR}/dist/resources/Shaders")

set(SPIRV_OUTPUTS "")
foreach (SHADER_FILE IN LISTS SHADER_FILES)
    get_filename_component(SHADER_NAME "${SHADER_FILE}" NAME)
    set(SPIRV_OUTPUT "${SHADERS_DIR}/${SHADER_NAME}.spv")
    add_custom_command(
            OUTPUT "${SPIRV_OUTPUT}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADERS_DIR}"
            COMMAND ${SLANGC_EXE} "${SHADER_FILE}" -target spirv -o "${SPIRV_OUTPUT}"
            DEPENDS "${SHADER_FILE}"
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
        URL https://github.com/zeux/volk/archive/refs/tags/1.4.350.tar.gz
        SYSTEM
)

target_compile_definitions(fate PRIVATE VK_NO_PROTOTYPES)

FetchContent_MakeAvailable(volk)
target_link_libraries(fate PRIVATE volk)

# VMA
FetchContent_Declare(
        vma
        URL https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.4.0.tar.gz
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
#set(KTX_FEATURE_TOOLS OFF)
#set(KTX_FEATURE_TESTS OFF)
#set(KTX_FEATURE_APPS OFF)
#
#FetchContent_MakeAvailable(ktx)
#target_link_libraries(fate PRIVATE ktx)

# SDL3
FetchContent_Declare(
        sdl3
        URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.16/SDL3-3.4.16.tar.gz
        SYSTEM
)

set(SDL_AUDIO OFF)
set(SDL_GPU OFF)
set(SDL_RENDER OFF)
set(SDL_CAMERA OFF)
set(SDL_JOYSTICK OFF)
set(SDL_HAPTIC OFF)
set(SDL_HIDAPI OFF)
set(SDL_POWER OFF)
set(SDL_SENSOR OFF)
set(SDL_DIALOG OFF)
set(SDL_TRAY OFF)

set(SDL_TEST_LIBRARY OFF)

FetchContent_MakeAvailable(sdl3)
target_link_libraries(fate PRIVATE SDL3::SDL3)

# GLM
FetchContent_Declare(
        glm
        URL https://github.com/g-truc/glm/archive/refs/tags/1.0.3.tar.gz
        SYSTEM
)

target_compile_definitions(fate PRIVATE GLM_FORCE_DEPTH_ZERO_TO_ONE)

FetchContent_MakeAvailable(glm)
target_link_libraries(fate PRIVATE glm::glm)

# ImGui
FetchContent_Declare(
        imgui
        URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9b-docking.tar.gz
)

FetchContent_MakeAvailable(imgui)

set(IMGUI_SOURCES
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui SYSTEM PUBLIC
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
        URL https://github.com/assimp/assimp/archive/refs/tags/v6.0.5.tar.gz
        SYSTEM
)

set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF)

set(ASSIMP_BUILD_GLTF_IMPORTER ON)

set(ASSIMP_BUILD_TESTS OFF)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF)
set(ASSIMP_INSTALL OFF)
set(ASSIMP_WARNINGS_AS_ERRORS OFF)
set(ASSIMP_BUILD_ZLIB ON)

FetchContent_MakeAvailable(assimp)

# assimp vendored zlib redefines fdopen to null causing some
# nonsense on some apple sdk versions(?) fix if they update to zlib 1.3.1?
if (APPLE AND TARGET zlibstatic)
    target_compile_definitions(zlibstatic PRIVATE fdopen=fdopen)
endif ()

# mitigate stricter transitive includes on llvm 23, remove once assimp updates
target_compile_options(assimp PRIVATE
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:Clang>>:-include ostream>
)

target_link_libraries(fate PRIVATE assimp::assimp)

# wuffs
FetchContent_Declare(
        wuffs
        URL https://raw.githubusercontent.com/google/wuffs/v0.4.0-alpha.10/release/c/wuffs-v0.4.c
        DOWNLOAD_NO_EXTRACT TRUE
        DOWNLOAD_NAME wuffs-v0.4.c
)

FetchContent_MakeAvailable(wuffs)
set_source_files_properties(${wuffs_SOURCE_DIR}/wuffs-v0.4.c PROPERTIES LANGUAGE CXX)

add_library(wuffs STATIC ${wuffs_SOURCE_DIR}/wuffs-v0.4.c)
target_compile_definitions(wuffs PRIVATE WUFFS_IMPLEMENTATION)

if (MSVC)
    target_compile_options(wuffs PRIVATE /W0)
else ()
    target_compile_options(wuffs PRIVATE -w)
endif ()

target_include_directories(wuffs PUBLIC ${wuffs_SOURCE_DIR})
target_link_libraries(fate PRIVATE wuffs)

# spdlog
FetchContent_Declare(
        spdlog
        URL https://github.com/gabime/spdlog/archive/refs/tags/v1.17.0.tar.gz
        SYSTEM
)

FetchContent_MakeAvailable(spdlog)
target_link_libraries(fate PRIVATE spdlog::spdlog)

# glaze
FetchContent_Declare(
        glaze
        URL https://github.com/stephenberry/glaze/archive/refs/tags/v8.3.0.tar.gz
        SYSTEM
)

FetchContent_MakeAvailable(glaze)
target_link_libraries(fate PRIVATE glaze::glaze)

# CLI11
FetchContent_Declare(
        CLI11
        URL https://github.com/CLIUtils/CLI11/releases/download/v2.7.2/CLI11-2.7.2-Source.tar.gz
        SYSTEM
)

FetchContent_MakeAvailable(CLI11)
target_link_libraries(fate PRIVATE CLI11::CLI11)