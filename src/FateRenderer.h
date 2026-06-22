#pragma once

#include <glad/glad.h>
#include "GLFW/glfw3.h"

#include <string_view>
#include <filesystem>
#include <vector>

#include "GPUMeshHandle.h"
#include "Scene.h"
#include "Vertex.h"

struct TransformBuffer {
    glm::mat4 modelMatrices[];
};

struct DrawElementsIndirectCommand {
    GLuint count;
    GLuint instanceCount;
    GLuint firstIndex;
    GLuint baseVertex;
    GLuint baseInstance;
};

class FateRenderer {
    static constexpr int DefaultBufferSize = 1024 * 1024 * 64; // 64mb

    static GLuint compileShader(GLuint type, std::string_view source);

    static GLuint loadShader(GLuint type, const std::filesystem::path& path);

    GLFWwindow* window;

    GLuint shaderProgram{};
    GLuint vbo{};
    GLuint ebo{};
    GLuint vao{};
    GLuint dib{};
    GLuint transformBufferSSBO{};

    std::vector<DrawElementsIndirectCommand> indirectBuffer{};
    std::vector<glm::mat4> modelMatrices{};

    std::size_t vboOffset{0};
    std::size_t eboOffset{0};

    glm::dvec3 cameraPosition{2.25f, 1.0f, 5.0f};
    glm::vec3 cameraRotation{0, 35.0f, 0};

public:
    FateRenderer();

    ~FateRenderer();

    void render(const Scene& scene);

    void drawEditorUI(const Scene& scene, double deltaTime);

    void endRender() const;

    [[nodiscard]] GLFWwindow* getWindow() const { return window; }

    GPUMeshHandle uploadMesh(const Mesh& mesh);
};
