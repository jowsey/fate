#pragma once

#include <glad/glad.h>
#include "GLFW/glfw3.h"

#include <filesystem>
#include <vector>

#include "GPUMeshHandle.h"
#include "Scene.h"
#include "TextureData.h"

struct DrawElementsIndirectCommand {
    GLuint count;
    GLuint instanceCount;
    GLuint firstIndex;
    GLuint baseVertex;
    GLuint baseInstance;
};

struct TransformBuffer {
    glm::mat4 modelMatrices[];
};

struct MaterialData {
    GLuint64 albedoMapHandle;
    float metallic;
    float roughness;
};

struct MaterialBuffer {
    MaterialData materials[];
};

class FateRenderer {
    static constexpr int DefaultBufferSize = 1024 * 1024 * 64; // 64mb

    static GLuint loadShader(GLuint type, const std::filesystem::path& path);

    GLFWwindow* window;

    GLuint shaderProgram{};
    GLuint vbo{};
    GLuint ebo{};
    GLuint vao{};
    GLuint dib{};
    GLuint transformBufferSSBO{};
    GLuint materialBufferSSBO{};

    std::vector<DrawElementsIndirectCommand> indirectBuffer{};
    std::vector<glm::mat4> modelMatrices{};
    std::vector<MaterialData> materials{};

    std::size_t vboOffset{0};
    std::size_t eboOffset{0};

    glm::dvec3 cameraPosition{2.25f, 1.0f, 5.0f};
    glm::vec3 cameraRotation{0, 35.0f, 0};

    GLuint64 missingTextureHandle;

public:
    FateRenderer();

    ~FateRenderer();

    void render(const Scene& scene);

    void drawEditorUI(const Scene& scene, double deltaTime);

    void endRender() const;

    [[nodiscard]] GLFWwindow* getWindow() const { return window; }

    GPUMeshHandle uploadMesh(const Mesh& mesh);

    GLuint64 uploadTexture(const TextureData& data, GLuint minFilter = GL_LINEAR, GLuint magFilter = GL_LINEAR);
};
