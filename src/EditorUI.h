#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Fate {
    class Renderer;
    class Scene;
    class SceneObject;
    class SceneTransform;

    class HierarchyWindow {
        void drawNode(const SceneTransform& transform, SceneObject*& selected);

        void drawSceneSettings(Renderer& renderer);

    public:
        void draw(const Scene& scene, SceneObject*& selected, Renderer& renderer);
    };

    class InspectorWindow {
    public:
        void draw(SceneObject* selected);
    };

    class AssetsWindow {
        struct Entry {
            std::filesystem::path path;
            std::string name;
            bool isDirectory{false};
        };

        std::filesystem::path projectDir;
        std::filesystem::path rootDir;
        std::filesystem::path currentDir;
        std::vector<Entry> entries;
        int selectedIndex{-1};
        bool scrollToSelection{false};

        void rescan();

        void goUp();

        void activate(int index);

        [[nodiscard]] std::string displayPath() const;

        static void openFile(const std::filesystem::path& path);

    public:
        void setProjectDirectory(const std::filesystem::path& dir);

        void draw();
    };

    class EditorUI {
        HierarchyWindow hierarchyWindow;
        InspectorWindow inspectorWindow;
        AssetsWindow assetsWindow;

        bool showResourceUsage{true};

        void drawMainMenuBar(double deltaTime);

        void drawResourceUsageWindow(const Renderer& renderer);

    public:
        inline static SceneObject* selectedObject{nullptr};

        void draw(const Scene* scene, Renderer& renderer, double deltaTime);

        void setProjectDirectory(const std::filesystem::path& dir) { assetsWindow.setProjectDirectory(dir); }
    };
}
