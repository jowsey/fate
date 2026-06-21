cmake_minimum_required(VERSION 3.31)
project(fate VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

include(FetchContent)

add_executable(fate
        src/main.cpp
        src/FateRenderer.cpp
        src/FateRenderer.h
        src/util/Utils.cpp
        src/util/Utils.h
        src/FateEngine.cpp
        src/FateEngine.h
        src/SceneObject.cpp
        src/SceneObject.h
        src/Scene.cpp
        src/Scene.h
        src/SceneTransform.cpp
        src/SceneTransform.h
        src/Vertex.cpp
        src/Vertex.h
        src/MeshHandle.cpp
        src/MeshHandle.h
        src/Mesh.cpp
        src/Mesh.h
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

# ImGui
FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.92.8-docking
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

target_link_libraries(imgui PRIVATE glfw)

target_link_libraries(fate PRIVATE imgui)

# CLI11
FetchContent_Declare(
        CLI11
        QUIET
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG v2.6.2
)

FetchContent_MakeAvailable(CLI11)
target_link_libraries(fate PRIVATE CLI11::CLI11)