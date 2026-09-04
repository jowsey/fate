#include "EditorUI.h"

#include <algorithm>
#include <deque>
#include <format>
#include <numeric>
#include <system_error>

#include <imgui.h>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_misc.h>

#include "spdlog/spdlog.h"

#include "Material.h"
#include "Mesh.h"
#include "Renderer.h"
#include "Scene.h"
#include "SceneObject.h"
#include "SceneTransform.h"
#include "utils/Files.h"

namespace {
    std::string toFileUri(const std::filesystem::path& absolutePath) {
        std::string path = absolutePath.generic_string();
        if (!path.starts_with('/')) {
            path = "/" + path;
        }

        std::string url = "file://";
        for (const unsigned char c: path) {
            const bool isValid = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
                                 || c == '-' || c == '_' || c == '.' || c == '~' || c == '/' || c == ':';
            if (isValid) {
                url += static_cast<char>(c);
            }
            else {
                url += std::format("%{:02X}", static_cast<unsigned>(c));
            }
        }
        return url;
    }
}

namespace Fate {
    void EditorUI::draw(const Scene* scene, Renderer& renderer, double deltaTime) {
        drawMainMenuBar(deltaTime);

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
        hierarchyWindow.draw(*scene, selectedObject, renderer);
        inspectorWindow.draw(selectedObject);
        assetsWindow.draw();

        drawResourceUsageWindow(renderer);
    }

    void EditorUI::drawMainMenuBar(const double deltaTime) {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Exit")) {
                    SDL_Event quitEvent;
                    quitEvent.type = SDL_EVENT_QUIT;
                    SDL_PushEvent(&quitEvent);
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("Resource Usage", nullptr, showResourceUsage)) {
                    showResourceUsage = !showResourceUsage;
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("View on GitHub")) {
                    SDL_OpenURL("https://github.com/jowsey/fate");
                }

                ImGui::Separator();

                ImGui::MenuItem("the Fate game engine", nullptr, nullptr, false);
                ImGui::MenuItem("v" FATE_VERSION, nullptr, nullptr, false);

                ImGui::EndMenu();
            }

            constexpr std::uint32_t frameBacklog{5};
            static std::deque<double> deltaTimeBuffer(frameBacklog);
            if (deltaTimeBuffer.size() >= frameBacklog) {
                deltaTimeBuffer.pop_front();
            }
            deltaTimeBuffer.push_back(deltaTime);

            const double averageDeltaTime = deltaTimeBuffer.empty()
                                                ? 0.0
                                                : std::accumulate(deltaTimeBuffer.begin(), deltaTimeBuffer.end(), 0.0) /
                                                  deltaTimeBuffer.size();

            const std::string fpsString = std::format("{:.3} fps", 1.0 / averageDeltaTime);
            const float fpsSize = ImGui::CalcTextSize(fpsString.c_str()).x;
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - fpsSize - 8.0f);
            ImGui::TextUnformatted(fpsString.c_str());

            ImGui::EndMainMenuBar();
        }
    }

    void EditorUI::drawResourceUsageWindow(const Renderer& renderer) {
        if (!showResourceUsage) return;

        ImGui::Begin("Resource Usage", &showResourceUsage);

        const auto bufferUsage = renderer.getGeometryBufferUsage();

        ImGui::ProgressBar(
            bufferUsage.vertexFraction,
            ImVec2(-1.0f, 0.0f),
            std::format(
                "Vertex buffer: {} ({:.3f}%)",
                FileUtils::prettyBytes(bufferUsage.vertexBytes),
                bufferUsage.vertexFraction * 100.0f
            ).c_str()
        );

        ImGui::ProgressBar(
            bufferUsage.indexFraction,
            ImVec2(-1.0f, 0.0f),
            std::format(
                "Index buffer: {} ({:.3f}%)",
                FileUtils::prettyBytes(bufferUsage.indexBytes),
                bufferUsage.indexFraction * 100.0f
            ).c_str()
        );

        // todo can we pull from descriptor set? or store manually again
        // ImGui::ProgressBar(
        //     0.0f,
        //     ImVec2(-1.0f, 0.0f),
        //     std::format("Texture usage: {}", FileUtils::prettyBytes(textureUploadedBytes)).c_str()
        // );

        ImGui::End();
    }

    void HierarchyWindow::draw(const Scene& scene, SceneObject*& selected, Renderer& renderer) {
        ImGui::Begin("Hierarchy");

        ImGui::DragScalarN("Camera position", ImGuiDataType_Double, &renderer.getCameraPosition().x, 3, 0.01f);
        ImGui::DragFloat3("Camera rotation", &renderer.getCameraRotation().x, 0.1f);
        ImGui::DragFloat("Camera FOV", &renderer.getCameraHorFovDegs(), 0.1f);

        ImGui::DragFloat3("Light direction", &renderer.getLightDir().x, 0.01f);
        ImGui::ColorEdit3("Light colour", &renderer.getLightColor().x);
        ImGui::DragFloat("Light intensity", &renderer.getLightIntensity(), 0.01f);

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Spacing();

        for (SceneObject* object: scene.getObjects()) {
            if (object->getTransform().getParent() != nullptr) continue;
            drawNode(object->getTransform(), selected);
        }

        ImGui::End();
    }

    void HierarchyWindow::drawNode(const SceneTransform& transform, SceneObject*& selected) {
        SceneObject& object = transform.getObject();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                   ImGuiTreeNodeFlags_SpanFullWidth;
        if (&object == selected) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        const bool hasChildren = !transform.getChildren().empty();
        if (!hasChildren) {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        const bool open = ImGui::TreeNodeEx(&transform, flags, "%s", object.getName().c_str());

        if (ImGui::IsItemClicked()) {
            selected = &object;
        }

        if (open && hasChildren) {
            for (const SceneTransform* childTransform: transform.getChildren()) {
                drawNode(*childTransform, selected);
            }

            ImGui::TreePop();
        }
    }

    void InspectorWindow::draw(SceneObject* selected) {
        ImGui::Begin("Inspector");

        if (!selected) {
            ImGui::End();
            return;
        }

        ImGui::TextUnformatted(selected->getName().c_str());
        ImGui::Separator();

        SceneTransform& transform = selected->getTransform();

        auto position = transform.getPosition();
        auto eulerAngles = transform.getEulerAngles();
        auto scale = transform.getLocalScale();

        if (ImGui::DragScalarN("Position", ImGuiDataType_Double, &position.x, 3, 0.01f)) {
            transform.setPosition(position);
        }
        if (ImGui::DragFloat3("Rotation", &eulerAngles.x, 0.1f)) {
            transform.setEulerAngles(eulerAngles);
        }
        if (ImGui::DragFloat3("Scale", &scale.x, 0.01f)) {
            transform.setLocalScale(scale);
        }

        for (std::size_t i = 0; i < selected->getMeshes().size(); ++i) {
            const auto mesh = selected->getMeshes()[i];
            const auto material = mesh->getMaterial();

            ImGui::SeparatorText(("Mesh " + std::to_string(i)).c_str());
            ImGui::Text("%zu vertices, %zu indices", mesh->getVertices().size(), mesh->getIndices().size());

            ImGui::SliderFloat("Metallic", &material->metallic, 0.0f, 1.0f);
            ImGui::SliderFloat("Roughness", &material->roughness, 0.0f, 1.0f);
            ImGui::ColorEdit4("Base colour", &material->baseColour.x);

            ImGui::Text("Albedo map:");
            ImGui::SameLine();
            ImGui::TextColored(material->albedoMap ? ImColor(0, 255, 0) : ImColor(255, 0, 0), "%s",
                               material->albedoMap ? "yes" : "no");
            ImGui::Text("Normal map:");
            ImGui::SameLine();
            ImGui::TextColored(material->normalMap ? ImColor(0, 255, 0) : ImColor(255, 0, 0), "%s",
                               material->normalMap ? "yes" : "no");
            ImGui::Text("Ambient map:");
            ImGui::SameLine();
            ImGui::TextColored(material->ambientMap ? ImColor(0, 255, 0) : ImColor(255, 0, 0), "%s",
                               material->ambientMap ? "yes" : "no");
            ImGui::Text("Roughness map:");
            ImGui::SameLine();
            ImGui::TextColored(material->roughnessMap ? ImColor(0, 255, 0) : ImColor(255, 0, 0), "%s",
                               material->roughnessMap ? "yes" : "no");
            ImGui::Text("Metallic map:");
            ImGui::SameLine();
            ImGui::TextColored(material->metallicMap ? ImColor(0, 255, 0) : ImColor(255, 0, 0), "%s",
                               material->metallicMap ? "yes" : "no");
            ImGui::Text("Emissive map:");
            ImGui::SameLine();
            ImGui::TextColored(material->emissiveMap ? ImColor(0, 255, 0) : ImColor(255, 0, 0), "%s",
                               material->emissiveMap ? "yes" : "no");
        }

        ImGui::End();
    }

    void AssetsWindow::setProjectDirectory(const std::filesystem::path& dir) {
        projectDir = dir;
        rootDir = dir / "Assets";
        currentDir = rootDir;
        selectedIndex = -1;
        rescan();
    }

    void AssetsWindow::draw() {
        if (!ImGui::Begin("Assets", nullptr, ImGuiWindowFlags_NoScrollbar)) {
            ImGui::End();
            return;
        }

        const bool canGoUp = !currentDir.empty() && currentDir != rootDir && currentDir.has_parent_path();

        ImGui::BeginDisabled(!canGoUp);
        if (ImGui::SmallButton("Up")) {
            goUp();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::SmallButton("Refresh")) {
            rescan();
        }

        const bool windowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

        if (canGoUp && windowFocused && ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
            goUp();
        }

        if (std::error_code ec; !std::filesystem::is_directory(currentDir, ec) || ec) {
            goUp();
            ImGui::End();
            return;
        }

        // reserve separator + one line of text
        const ImGuiStyle& style = ImGui::GetStyle();
        const float footerHeight = style.ItemSpacing.y + ImGui::GetTextLineHeightWithSpacing();
        ImGui::BeginChild("AssetList", ImVec2(0, -footerHeight));

        ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
        for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
            ImGui::PushID(i);

            const bool isSelected = i == selectedIndex;
            if (ImGui::Selectable(entries[i].name.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
                selectedIndex = i;
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    activate(i);
                }
            }

            if (isSelected && scrollToSelection) {
                ImGui::SetScrollHereY();
            }

            ImGui::PopID();
        }
        ImGui::PopItemFlag();
        scrollToSelection = false;

        ImGui::EndChild();

        if (windowFocused && !entries.empty()) {
            const int lastIndex = static_cast<int>(entries.size()) - 1;
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
                selectedIndex = std::min(selectedIndex + 1, lastIndex);
                scrollToSelection = true;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
                selectedIndex = selectedIndex < 0 ? lastIndex : std::max(selectedIndex - 1, 0);
                scrollToSelection = true;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
                if (selectedIndex >= 0) {
                    activate(selectedIndex);
                }
            }
        }

        // footer
        ImGui::Separator();

        const std::string path = displayPath();
        const bool truncated = ImGui::CalcTextSize(path.c_str()).x > ImGui::GetContentRegionAvail().x;
        ImGui::TextDisabled("%s", path.c_str());
        if (truncated) {
            ImGui::SetItemTooltip("%s", path.c_str());
        }

        ImGui::End();
    }

    void AssetsWindow::rescan() {
        entries.clear();

        if (!currentDir.empty()) {
            std::error_code ec;
            std::filesystem::directory_iterator it(currentDir, ec);
            if (!ec) {
                for (const auto& dirEntry: it) {
                    Entry entry;
                    entry.path = dirEntry.path();
                    entry.name = entry.path.filename().string();
                    entry.isDirectory = dirEntry.is_directory(ec);
                    ec.clear();
                    entries.push_back(std::move(entry));
                }
            }
        }

        std::ranges::sort(entries, [](const Entry& a, const Entry& b) {
            if (a.isDirectory != b.isDirectory) {
                return a.isDirectory > b.isDirectory;
            }
            return a.name < b.name;
        });

        if (selectedIndex >= static_cast<int>(entries.size())) {
            selectedIndex = -1;
        }
    }

    void AssetsWindow::goUp() {
        currentDir = currentDir.parent_path();
        selectedIndex = -1;
        rescan();
    }

    void AssetsWindow::activate(const int index) {
        if (index < 0 || index >= static_cast<int>(entries.size())) {
            return;
        }

        const Entry entry = entries[index];
        if (entry.isDirectory) {
            currentDir = entry.path;
            selectedIndex = -1;
            rescan();
        }
        else {
            openFile(entry.path);
        }
    }

    std::string AssetsWindow::displayPath() const {
        std::filesystem::path target = currentDir;
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(entries.size())) {
            target = entries[selectedIndex].path;
        }

        if (!projectDir.empty()) {
            std::error_code ec;
            const std::filesystem::path rel = std::filesystem::relative(target, projectDir, ec);
            if (!ec && !rel.empty()) {
                return rel.generic_string();
            }
        }
        return target.generic_string();
    }

    void AssetsWindow::openFile(const std::filesystem::path& path) {
        std::error_code ec;
        const std::filesystem::path absolute = std::filesystem::absolute(path, ec);
        if (ec) {
            spdlog::warn("AssetsWindow failed to resolve resolve asset path '{}'", path.string());
            return;
        }

        if (!SDL_OpenURL(toFileUri(absolute).c_str())) {
            spdlog::warn("AssetsWindow failed to open '{}': {}", path.string(), SDL_GetError());
        }
    }
}
