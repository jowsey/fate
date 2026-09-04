#include "Editor.h"

#include <expected>
#include <format>
#include <stdexcept>

#include <glaze/yaml.hpp>

#include "spdlog/spdlog.h"

#include "Engine.h"

namespace Fate {
    static std::expected<FateProject, std::string> loadProjectFile(const std::filesystem::path& projectPath) {
        std::filesystem::path filePath;

        if (std::filesystem::is_directory(projectPath)) {
            filePath = projectPath / ".fateproject";
        }
        else if (projectPath.filename() == ".fateproject") {
            filePath = projectPath;
        }
        else {
            return std::unexpected("not a .fateproject file");
        }

        if (std::filesystem::exists(filePath)) {
            FateProject project{};
            if (auto ec = glz::read_file_yaml(project, filePath.string())) {
                std::string err = glz::format_error(ec);
                return std::unexpected(std::format("failed to read file: {}", err));
            }

            return project;
        }

        return std::unexpected(".fateproject not found");
    }

    Editor::Editor(const std::filesystem::path& projectPath) {
        auto loaded = loadProjectFile(projectPath);
        if (!loaded) {
            throw std::runtime_error(std::format("Failed to load project file at {}: {}!", projectPath.string(), loaded.error()));
        }
        project = std::move(*loaded);

        if (std::filesystem::is_directory(projectPath)) {
            projectDirectory = std::filesystem::absolute(projectPath);
        }
        else {
            projectDirectory = std::filesystem::absolute(projectPath.parent_path());
        }

        spdlog::info("Loading project '{}'", project.name);

        if (project.engineVersion != FATE_VERSION) {
            spdlog::warn("Project expects fate {}, you are using {}!", project.engineVersion, FATE_VERSION);
        }

        engine = std::make_unique<Engine>(project.name);
        ui.setProjectDirectory(projectDirectory);
    }

    Editor::~Editor() = default;

    void Editor::run() {
        while (engine->beginFrame()) {
            ui.draw(engine->getActiveScene(), engine->getRenderer(), engine->getDeltaTime());
            engine->endFrame();
        }
    }
}
