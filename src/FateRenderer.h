#pragma once

#include <glad/glad.h>
#include "GLFW/glfw3.h"

#include <string_view>
#include <__filesystem/path.h>

class FateRenderer {
    static GLuint compileShader(GLuint type, std::string_view source);

    static GLuint loadShader(GLuint type, const std::filesystem::path&path);

    GLFWwindow* window;

    GLuint shaderProgram{};
    GLuint vao{};
    GLuint vbo{};

public:
    FateRenderer();

    ~FateRenderer();

    void render() const;

    [[nodiscard]] GLFWwindow* getWindow() const { return window; }
};
