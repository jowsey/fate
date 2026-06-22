#include "FateRenderer.h"

#include <print>
#include <cwchar>
#include <deque>
#include <fstream>
#include <iostream>
#include <numeric>
#include <vector>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.inl"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "GPUMeshHandle.h"
#include "TextureData.h"
#include "Scene.h"
#include "utils/Files.h"
#include "utils/Paths.h"

void APIENTRY glDebugOutput(GLenum source, GLenum type, unsigned int id, GLenum severity, GLsizei length, const char* message, const void* userParam) {
    // suppress non-significant
    if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return;

    std::println(stderr, "OpenGL debug message ({}): {}", id, message);
}

GLuint FateRenderer::loadShader(const GLuint type, const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open shader file: " + path.string());
    }

    const std::string source((std::istreambuf_iterator(file)), std::istreambuf_iterator<char>());
    const char* sourceData = source.data();

    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &sourceData, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        std::array<char, 512> infoLog{};
        glGetShaderInfoLog(shader, infoLog.size(), nullptr, infoLog.data());
        std::println(stderr, "shader compilation failed for {}: {}", path.filename().string(), infoLog.data());
    }

    return shader;
}

FateRenderer::FateRenderer() {
    // glfw window setup
    if (!glfwInit()) throw std::runtime_error("failed to initialize GLFW");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

    window = glfwCreateWindow(1280, 720, "fate", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("failed to create GLFW window");
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        throw std::runtime_error("failed to initialize GLAD");
    }

    // enable debug if available
    int flags;
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(glDebugOutput, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
    }

    // shaders
    const auto shaderPath = PathUtils::getEnginePath() / "resources/Shaders";
    const GLuint vertexShader = loadShader(GL_VERTEX_SHADER, shaderPath / "lit.vert");
    const GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, shaderPath / "lit.frag");

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // buffers

    // vertex buffer
    glCreateBuffers(1, &vbo);
    glNamedBufferStorage(vbo, DefaultBufferSize, nullptr, GL_DYNAMIC_STORAGE_BIT);

    // element buffer
    glCreateBuffers(1, &ebo);
    glNamedBufferStorage(ebo, DefaultBufferSize, nullptr, GL_DYNAMIC_STORAGE_BIT);

    // vertex array
    glCreateVertexArrays(1, &vao);

    glVertexArrayVertexBuffer(vao, 0, vbo, 0, sizeof(Vertex));
    glVertexArrayElementBuffer(vao, ebo);

    // baseColour
    glEnableVertexArrayAttrib(vao, 0);
    glVertexArrayAttribFormat(vao, 0, 4, GL_FLOAT, GL_FALSE, 0 * sizeof(float));
    glVertexArrayAttribBinding(vao, 0, 0);

    // position
    glEnableVertexArrayAttrib(vao, 1);
    glVertexArrayAttribFormat(vao, 1, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float));
    glVertexArrayAttribBinding(vao, 1, 0);

    // normal
    glEnableVertexArrayAttrib(vao, 2);
    glVertexArrayAttribFormat(vao, 2, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float));
    glVertexArrayAttribBinding(vao, 2, 0);

    // texCoord
    glEnableVertexArrayAttrib(vao, 3);
    glVertexArrayAttribFormat(vao, 3, 2, GL_FLOAT, GL_FALSE, 10 * sizeof(float));
    glVertexArrayAttribBinding(vao, 3, 0);

    // TransformBuffer SSBO
    glCreateBuffers(1, &transformBufferSSBO);
    glNamedBufferStorage(transformBufferSSBO, 128 * sizeof(glm::mat4), nullptr, GL_DYNAMIC_STORAGE_BIT);

    // MaterialBuffer SSBO
    glCreateBuffers(1, &materialBufferSSBO); // todo figure out what we're doing about buffer resizing
    glNamedBufferStorage(materialBufferSSBO, 128 * sizeof(MaterialData), nullptr, GL_DYNAMIC_STORAGE_BIT);

    // draw indirect buffer
    glCreateBuffers(1, &dib);
    glNamedBufferStorage(dib, 128 * sizeof(DrawElementsIndirectCommand), nullptr, GL_DYNAMIC_STORAGE_BIT);

    // default assets
    std::uint32_t missingTextureWidth, missingTextureHeight;
    auto missingTexture = FileUtils::loadPngFromFile(
        PathUtils::getEnginePath() / "resources/Textures/missing.png",
        missingTextureWidth,
        missingTextureHeight
    );
    missingTextureHandle = uploadTexture({missingTextureWidth, missingTextureHeight, missingTexture.get()}, GL_NEAREST, GL_NEAREST);

    // imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    io.Fonts->AddFontFromFileTTF((PathUtils::getEnginePath() / "resources/Fonts/Inter_18pt-Regular.ttf").string().c_str());

    constexpr auto bgLight = ImColor(42, 42, 42);
    constexpr auto bgDark = ImColor(26, 26, 26);
    constexpr auto border = ImColor(64, 64, 64);

    ImGuiStyle& style = ImGui::GetStyle();
    style.FontSizeBase = 16.0f;

    style.WindowRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.FrameRounding = 2.0f;

    style.PopupBorderSize = 0.0f;

    style.Colors[ImGuiCol_WindowBg] = bgLight;
    style.Colors[ImGuiCol_TitleBg] = bgDark;
    style.Colors[ImGuiCol_TitleBgActive] = bgDark;
    style.Colors[ImGuiCol_MenuBarBg] = bgDark;
    style.Colors[ImGuiCol_Border] = border;
    style.Colors[ImGuiCol_PopupBg] = bgDark;

    float contentScale;
    glfwGetWindowContentScale(window, &contentScale, nullptr);
    style.FontScaleDpi = contentScale;
    style.ScaleAllSizes(contentScale);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();

    // set window icon
    auto windowIcon = GLFWimage();

    std::uint32_t iconWidth, iconHeight;
    const auto iconPixels = FileUtils::loadPngFromFile(
        PathUtils::getEnginePath() / "resources/Textures/fate-icon.png",
        iconWidth,
        iconHeight
    );

    windowIcon.width = static_cast<int>(iconWidth);
    windowIcon.height = static_cast<int>(iconHeight);
    windowIcon.pixels = iconPixels.get();
    glfwSetWindowIcon(window, 1, &windowIcon);
    glfwPollEvents(); // required, windows bug
}

FateRenderer::~FateRenderer() {
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &dib);
    glDeleteBuffers(1, &transformBufferSSBO);
    glDeleteBuffers(1, &materialBufferSSBO);
    glDeleteProgram(shaderProgram);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
}

void DrawSceneHierarchyNode(SceneTransform& transform) {
    if (ImGui::TreeNode(transform.getObject().getName().c_str())) {
        auto position = transform.getPosition();

        if (ImGui::DragScalarN("Position", ImGuiDataType_Double, &position, 3, 0.01f)) {
            transform.setPosition(position);
        }

        for (std::size_t i = 0; i < transform.getObject().getMeshes().size(); ++i) {
            const auto mesh = transform.getObject().getMeshes()[i];
            const auto material = mesh->getMaterial();

            if (ImGui::CollapsingHeader(("Mesh " + std::to_string(i)).c_str())) {
                ImGui::Text("%zu vertices, %zu indices", mesh->getVertices().size(), mesh->getIndices().size());
                ImGui::Text("Has albedo map: %s", material->albedoMapHandle.has_value() ? "true" : "false");
                ImGui::Text("Metallic: %.3f, Roughness: %.3f", material->metallic, material->roughness);
            }
        }

        for (SceneTransform* childTransform: transform.getChildren()) {
            DrawSceneHierarchyNode(*childTransform);
        }

        ImGui::TreePop();
    }
}

void FateRenderer::render(const Scene& scene) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
    glClearDepth(1.0f);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    int winWidth;
    int winHeight;
    glfwGetFramebufferSize(window, &winWidth, &winHeight);
    const float winAspect = static_cast<float>(winWidth) / winHeight;

    // todo should probably only do this on glfw resize
    glViewport(0, 0, winWidth, winHeight);

    constexpr float camHorFovDegs = 60.0f;
    const float fovYRads = 2.0f * glm::atan(glm::tan(glm::radians(camHorFovDegs) * 0.5f) / winAspect);
    const glm::mat4 proj = glm::perspective(fovYRads, winAspect, 0.01f, 100.0f);

    const glm::mat4 view = glm::inverse(glm::translate(glm::mat4(1.0f), glm::vec3(cameraPosition)) * glm::mat4_cast(glm::quat(glm::radians(cameraRotation))));

    std::vector<RenderItem> renderItems; // todo cache + reserve, maybe store total meshes

    const auto& objects = scene.getObjects();

    // todo we need to migrate to camera-relative positions at some point
    const auto camPos = glm::vec3(cameraPosition);

    for (const auto object: objects) {
        const auto& meshes = object->getMeshes();

        for (const auto& mesh: meshes) {
            // todo almost certainly doesn't need to be rebuilt from scratch every frame
            const auto command = DrawElementsIndirectCommand{
                .count = static_cast<GLuint>(mesh->getIndices().size()),
                .instanceCount = 1,
                .firstIndex = mesh->getGPUHandle()->getEboOffset(),
                .baseVertex = mesh->getGPUHandle()->getVboOffset(),
                .baseInstance = 0
            };

            const auto modelMatrix = object->getTransform().getWorldMatrix();
            const auto meshPosition = glm::vec3(modelMatrix[3]);

            const auto meshMaterial = mesh->getMaterial();
            const auto materialData = MaterialData{
                .baseColour = meshMaterial->baseColour,
                .albedoMapHandle = meshMaterial->albedoMapHandle.value_or(missingTextureHandle),
                .mapFlags = meshMaterial->mapFlags,
                .metallic = meshMaterial->metallic,
                .roughness = meshMaterial->roughness
            };

            renderItems.push_back(RenderItem{
                .command = command,
                .modelMatrix = modelMatrix,
                .material = materialData,
                .distance = glm::length(meshPosition - camPos),
                .isTransparent = meshMaterial->useAlpha
            });
        }
    }

    std::ranges::sort(renderItems, [](const RenderItem& a, const RenderItem& b) {
        if (a.isTransparent != b.isTransparent) {
            return !a.isTransparent; // all opaque first
        }

        // opaque is closest-first, transparent is farthest-first
        return !a.isTransparent
                   ? a.distance < b.distance
                   : a.distance > b.distance;
    });

    indirectBuffer.clear();
    modelMatrices.clear();
    materials.clear();
    indirectBuffer.reserve(renderItems.size());
    modelMatrices.reserve(renderItems.size());
    materials.reserve(renderItems.size());

    std::uint32_t ssboIndex = 0;
    std::uint32_t opaqueCount = 0;
    std::uint32_t transparentCount = 0;

    for (auto& item: renderItems) {
        item.command.baseInstance = ssboIndex++;

        modelMatrices.push_back(item.modelMatrix);
        materials.push_back(item.material);

        indirectBuffer.push_back(item.command);

        if (item.isTransparent) {
            transparentCount++;
        }
        else {
            opaqueCount++;
        }
    }

    // todo we also definitely don't need to be resending everything every frame
    glNamedBufferSubData(transformBufferSSBO, 0, modelMatrices.size() * sizeof(glm::mat4), modelMatrices.data());
    glNamedBufferSubData(materialBufferSSBO, 0, materials.size() * sizeof(MaterialData), materials.data());

    glm::mat4 vp = proj * view;
    glProgramUniformMatrix4fv(shaderProgram, 0, 1, GL_FALSE, glm::value_ptr(vp));

    glUseProgram(shaderProgram);
    glBindVertexArray(vao);

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, dib);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, transformBufferSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, materialBufferSSBO);

    glEnable(GL_DEPTH_TEST);

    glNamedBufferSubData(dib, 0, indirectBuffer.size() * sizeof(DrawElementsIndirectCommand), indirectBuffer.data());

    // Opaque pass
    if (opaqueCount > 0) {
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);

        glMultiDrawElementsIndirect(
            GL_TRIANGLES,
            GL_UNSIGNED_INT,
            nullptr,
            opaqueCount,
            0
        );
    }

    // Transparent pass
    if (transparentCount > 0) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);

        glMultiDrawElementsIndirect(
            GL_TRIANGLES,
            GL_UNSIGNED_INT,
            reinterpret_cast<const void *>(opaqueCount * sizeof(DrawElementsIndirectCommand)),
            transparentCount,
            0
        );
    }
}

void FateRenderer::drawEditorUI(const Scene& scene, const double deltaTime) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit")) {
                glfwSetWindowShouldClose(window, true);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("View on GitHub")) {
                PathUtils::openBrowser("https://github.com/jowsey/fate");
            }

            ImGui::Separator();

            ImGui::MenuItem("the Fate game engine", nullptr, nullptr, false);
            ImGui::MenuItem("v" FATE_VERSION, nullptr, nullptr, false);

            ImGui::EndMenu();
        }

        static std::deque<double> deltaTimeBuffer{};
        if (deltaTimeBuffer.size() >= 30) {
            deltaTimeBuffer.pop_front();
        }
        deltaTimeBuffer.push_back(deltaTime);

        const double averageDeltaTime = deltaTimeBuffer.empty()
                                            ? 0.0
                                            : std::accumulate(deltaTimeBuffer.begin(), deltaTimeBuffer.end(), 0.0) / deltaTimeBuffer.size();

        const std::string fpsString = std::format("{:.3} fps", 1.0 / averageDeltaTime);
        const float fpsSize = ImGui::CalcTextSize(fpsString.c_str()).x;
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - fpsSize - 8.0f);
        ImGui::TextUnformatted(fpsString.c_str());

        ImGui::EndMainMenuBar();
    }

    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    ImGui::Begin("Debug");

    ImGui::DragScalarN("Camera position", ImGuiDataType_Double, &cameraPosition, 3, 0.01f);
    ImGui::DragFloat3("Camera rotation", &cameraRotation.x, 0.01f);

    if (ImGui::CollapsingHeader("Hierarchy", nullptr, ImGuiTreeNodeFlags_DefaultOpen)) {
        for (SceneObject* object: scene.getObjects()) {
            if (object->getTransform().getParent() != nullptr) continue;
            DrawSceneHierarchyNode(object->getTransform());
        }
    }

    if (ImGui::CollapsingHeader("Resource usage", nullptr, ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ProgressBar(
            static_cast<float>(vboOffset) / DefaultBufferSize,
            ImVec2(-1.0f, 0.0f),
            std::format("VBO usage: {} ({:.5f}%)", FileUtils::prettyBytes(vboOffset), static_cast<float>(vboOffset) / DefaultBufferSize * 100.0f).c_str()
        );

        ImGui::ProgressBar(
            static_cast<float>(eboOffset) / DefaultBufferSize,
            ImVec2(-1.0f, 0.0f),
            std::format("EBO usage: {} ({:.5f}%)", FileUtils::prettyBytes(eboOffset), static_cast<float>(eboOffset) / DefaultBufferSize * 100.0f).c_str()
        );
    }

    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void FateRenderer::endRender() const {
    glfwSwapBuffers(window);
}

GPUMeshHandle FateRenderer::uploadMesh(const Mesh& mesh) {
    const auto vboBytesNeeded = mesh.getVertices().size() * sizeof(Vertex);
    const auto eboBytesNeeded = mesh.getIndices().size() * sizeof(std::uint32_t);

    // todo store all active allocations and look for cleared spots, then in worst case, make a new pool and batch per-pool
    if (vboOffset + vboBytesNeeded > DefaultBufferSize) {
        throw std::runtime_error("VBO buffer overflow, see todo");
    }

    if (eboOffset + eboBytesNeeded > DefaultBufferSize) {
        throw std::runtime_error("EBO buffer overflow, see todo");
    }

    std::println("Uploading mesh with {} vertices ({}) and {} indices ({})",
                 mesh.getVertices().size(), FileUtils::prettyBytes(vboBytesNeeded),
                 mesh.getIndices().size(), FileUtils::prettyBytes(eboBytesNeeded)
    );

    glNamedBufferSubData(vbo, vboOffset, mesh.getVertices().size() * sizeof(Vertex), mesh.getVertices().data());
    glNamedBufferSubData(ebo, eboOffset, mesh.getIndices().size() * sizeof(std::uint32_t), mesh.getIndices().data());

    const std::size_t vboStoredIndex = vboOffset;
    const std::size_t eboStoredIndex = eboOffset;

    vboOffset += vboBytesNeeded;
    eboOffset += eboBytesNeeded;

    return GPUMeshHandle(vboStoredIndex / sizeof(Vertex), eboStoredIndex / sizeof(std::uint32_t));
}

GLuint64 FateRenderer::uploadTexture(const TextureData& data, const GLuint minFilter, const GLuint magFilter) {
    GLuint texture;

    std::println("Uploading texture of size {}x{} ({})", data.width, data.height, FileUtils::prettyBytes(data.width * data.height * 4));

    glCreateTextures(GL_TEXTURE_2D, 1, &texture);
    glTextureStorage2D(texture, 1, GL_RGBA8, data.width, data.height);
    glTextureSubImage2D(texture, 0, 0, 0, data.width, data.height, GL_RGBA, GL_UNSIGNED_BYTE, data.pixels);
    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, minFilter);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, magFilter);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_REPEAT);

    const GLuint64 textureHandle = glGetTextureHandleARB(texture);
    glMakeTextureHandleResidentARB(textureHandle);

    return textureHandle;
}
