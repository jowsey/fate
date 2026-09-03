#pragma once

#include <filesystem>
#include <memory>

#include "EditorUI.h"
#include "FateProject.h"

namespace Fate {
    class Engine;

    class Editor {
        FateProject project;
        std::filesystem::path projectDirectory;
        std::unique_ptr<Engine> engine;
        EditorUI ui;

    public:
        explicit Editor(const std::filesystem::path& projectPath);

        ~Editor();

        void run();

        [[nodiscard]] const std::filesystem::path& getProjectDirectory() const { return projectDirectory; }
    };
}
