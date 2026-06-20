#pragma once

#include <glad/glad.h>
#include "GLFW/glfw3.h"

#include <string_view>
#include <filesystem>
#include <vector>

#include "MeshHandle.h"
#include "Scene.h"
#include "Vertex.h"

class FateRenderer {
    static constexpr int DefaultBufferSize = 1024 * 1024 * 64; // 64mb

    static GLuint compileShader(GLuint type, std::string_view source);

    static GLuint loadShader(GLuint type, const std::filesystem::path&path);

    GLFWwindow* window;

    GLuint shaderProgram{};
    GLuint vbo{};
    GLuint ebo{};
    GLuint vao{};

    std::size_t vboOffset{0};
    std::size_t eboOffset{0};

public:
    FateRenderer();

    ~FateRenderer();

    void render(const Scene&scene) const;

    [[nodiscard]] GLFWwindow* getWindow() const { return window; }

    MeshHandle uploadMesh(const std::vector<Vertex>&vertices, const std::vector<unsigned int>&indices);
};
