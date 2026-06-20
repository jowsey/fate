#include <print>
#include <cwchar>

#include "FateRenderer.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.inl"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "util/Utils.h"

void APIENTRY glDebugOutput(GLenum source, GLenum type, unsigned int id, GLenum severity, GLsizei length, const char* message, const void* userParam) {
    // suppress non-significant
    if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return;

    std::println(stderr, "OpenGL debug message ({}): {}", id, message);
}

GLuint FateRenderer::compileShader(const GLuint type, const std::string_view source) {
    const char* src = source.data();

    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        std::array<char, 512> infoLog{};
        glGetShaderInfoLog(shader, infoLog.size(), nullptr, infoLog.data());
        std::println(stderr, "Shader compilation failed: {}", infoLog.data());
    }

    return shader;
}

GLuint FateRenderer::loadShader(const GLuint type, const std::filesystem::path&path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open shader file: " + path.string());
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return compileShader(type, buffer.str());
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
    const auto shaderPath = getExecutablePath().parent_path().parent_path() / "resources/Shaders";
    const GLuint vertexShader = loadShader(GL_VERTEX_SHADER, shaderPath / "lit.vert");
    const GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, shaderPath / "lit.frag");

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // vertices
    constexpr std::array<float, 6 * 3> vertices = {
        0.0f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f,
        -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
    };

    glCreateVertexArrays(1, &vao);
    glCreateBuffers(1, &vbo);

    glNamedBufferStorage(vbo, vertices.size() * sizeof(float), vertices.data(), 0);
    glVertexArrayVertexBuffer(vao, 0, vbo, 0, 6 * sizeof(float));

    glEnableVertexArrayAttrib(vao, 0);
    glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(vao, 0, 0);

    glEnableVertexArrayAttrib(vao, 1);
    glVertexArrayAttribFormat(vao, 1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float));
    glVertexArrayAttribBinding(vao, 1, 0);

    glEnable(GL_DEPTH_TEST);

    // imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO&io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    io.Fonts->AddFontFromFileTTF(getExecutablePath().parent_path().parent_path().append("resources/Fonts/Inter_18pt-Regular.ttf").string().c_str());

    constexpr auto bgLight = ImColor(42, 42, 42);
    constexpr auto bgDark = ImColor(26, 26, 26);
    constexpr auto border = ImColor(64, 64, 64);

    ImGuiStyle&style = ImGui::GetStyle();
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
}

FateRenderer::~FateRenderer() {
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(shaderProgram);

    glfwTerminate();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void FateRenderer::render() const {
    glfwPollEvents();

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    int winWidth;
    int winHeight;
    glfwGetFramebufferSize(window, &winWidth, &winHeight);
    const float winAspect = static_cast<float>(winWidth) / winHeight;

    const auto time = static_cast<float>(glfwGetTime());

    constexpr float camHorFovDegs = 60.0f;
    const float fovYRads = 2.0f * glm::atan(glm::tan(glm::radians(camHorFovDegs) * 0.5f) / winAspect);

    const glm::mat4 proj = glm::perspective(fovYRads, winAspect, 0.01f, 100.0f);
    const glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -2.5f));
    const glm::mat4 model = glm::rotate(glm::mat4(1.0f), time, glm::vec3(0.0f, 1.0f, 0.0f));

    glm::mat4 mvp = proj * view * model;

    glProgramUniformMatrix4fv(shaderProgram, 0, 1, GL_FALSE, glm::value_ptr(mvp));

    glUseProgram(shaderProgram);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit")) {
                glfwSetWindowShouldClose(window, true);
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    ImGui::Begin("fate");

    ImGui::Text("hi guys");
    if (ImGui::Button("press me")) {
        ImGui::Text("button pressed");
    }

    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
}
