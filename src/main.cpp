#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include <CLI/CLI.hpp>

#include <SDL3/SDL_process.h>

#include "spdlog/spdlog.h"

#include <glaze/yaml.hpp>

#include "Engine.h"
#include "FateProject.h"
#include "Scene.h"
#include "utils/Paths.h"

#ifdef _WIN32
#include <Windows.h>
#endif

int main(const int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

#ifndef NDEBUG // double negative </3
    spdlog::set_level(spdlog::level::debug);
#endif

    CLI::App app;

    app.name("fate");
    app.description("The Fate game engine, v" FATE_VERSION);
    app.footer("Learn more: https://github.com/jowsey/fate");
    app.set_version_flag("-v,--version", FATE_VERSION);

    CLI::App* initCmd = app.add_subcommand("init", "Initialize a new project");
    initCmd->add_option("name", "Name of new project")->required();

    CLI::App* openCmd = app.add_subcommand("open", "Open an existing project");
    openCmd->add_option("path", "Path of fate project to open")->required();

    app.require_subcommand(1);

    CLI11_PARSE(app, argc, argv);

    if (initCmd->parsed()) {
        const auto name = initCmd->get_option("name")->as<std::string>();
        const auto relativePath = std::filesystem::path(name);
        const auto absolutePath = std::filesystem::absolute(relativePath);

        if (std::filesystem::exists(absolutePath) && !std::filesystem::is_empty(absolutePath)) {
            spdlog::error("Cannot create project at {}: directory is not empty.", absolutePath.string());
            return 1;
        }

        spdlog::info("Initializing project '{}' at {}", name, absolutePath.string());

        // Build project directory
        if (!std::filesystem::exists(absolutePath)) {
            std::filesystem::create_directories(absolutePath);
        }

        std::filesystem::copy(Fate::PathUtils::getEnginePath() / "resources/ProjectTemplates/Default", absolutePath, std::filesystem::copy_options::recursive);

        // Replace placeholder tags
        const auto TagReplacements = std::to_array<std::pair<std::string, std::string>>({
            {"{{ProjectName}}", name},
            {"{{FateVersion}}", FATE_VERSION}
        });

        for (const auto& entry: std::filesystem::recursive_directory_iterator(absolutePath)) {
            if (!entry.is_regular_file()) continue;

            const std::filesystem::path& filePath = entry.path();

            // Find in file content
            if (std::ifstream inFile{filePath, std::ios::binary | std::ios::ate}) {
                std::string content(inFile.tellg(), '\0');
                inFile.seekg(0);
                inFile.read(content.data(), content.size());
                inFile.close();

                for (const auto& [tag, replacement]: TagReplacements) {
                    if (const std::size_t contentPos = content.find(tag); contentPos != std::string::npos) {
                        content.replace(contentPos, tag.length(), replacement);

                        if (std::ofstream outFile{filePath, std::ios::binary | std::ios::trunc}) {
                            outFile.write(content.data(), content.size());
                        }
                    }
                }
            }

            // Find in file name
            for (const auto& [tag, replacement]: TagReplacements) {
                const std::string filename = filePath.filename().string();

                if (const std::size_t pos = filename.find(tag); pos != std::string::npos) {
                    std::string newFilename = filename;
                    newFilename.replace(pos, tag.length(), replacement);

                    const std::filesystem::path newFilePath = filePath.parent_path() / newFilename;
                    std::filesystem::rename(filePath, newFilePath);
                    break;
                }
            }
        }

        // Initialize git repo
        const std::string pathStr = absolutePath.string();
        const char* initArgs[] = {"git", "-C", pathStr.c_str(), "init", "--quiet", nullptr};
        if (SDL_Process* initProc = SDL_CreateProcess(initArgs, false)) {
            spdlog::info("Initialising new git repository");

            SDL_WaitProcess(initProc, true, nullptr);
            SDL_DestroyProcess(initProc);
        }

        return 0;
    }

    if (openCmd->parsed()) {
        auto projectPath = openCmd->get_option("path")->as<std::filesystem::path>();

        // Find .fateproject
        std::filesystem::path fateProjectPath;
        if (std::filesystem::is_directory(projectPath)) {
            fateProjectPath = projectPath / ".fateproject";
            if (!std::filesystem::exists(fateProjectPath)) {
                for (const auto& entry: std::filesystem::recursive_directory_iterator(projectPath)) {
                    if (!entry.is_regular_file() || entry.path().filename() != ".fateproject") continue;

                    fateProjectPath = entry.path();
                    break;
                }
            }
        }
        else {
            if (projectPath.filename() == ".fateproject") {
                fateProjectPath = projectPath;
            }
            else if (!std::filesystem::exists(projectPath) || std::filesystem::is_directory(projectPath)) {
                fateProjectPath = projectPath / ".fateproject";
            }
        }

        if (std::filesystem::exists(fateProjectPath)) {
            std::string buffer;
            std::ifstream inFile(fateProjectPath);
            if (!inFile) {
                spdlog::error("Failed to open {}", fateProjectPath.string());
                return 1;
            }

            buffer.assign(std::istreambuf_iterator<char>(inFile), std::istreambuf_iterator<char>());
            inFile.close();

            FateProject project{};

            if (auto ec = glz::read_yaml(project, buffer)) {
                std::string err = glz::format_error(ec, buffer);
                spdlog::error("Failed to parse {} ({})", fateProjectPath.string(), err);
                return 1;
            }

            spdlog::info("Loading project '{}'", project.name);
            if (project.engineVersion != FATE_VERSION) {
                spdlog::warn("Project expects fate {}, you are using {}!", project.engineVersion, FATE_VERSION);
            }
        }
        else {
            spdlog::error("Failed to find {} (not a project directory?)", fateProjectPath.string());
            return 1;
        }

        // Main engine loop
        Fate::Engine engine;

        auto mainScene = std::make_unique<Fate::Scene>("Main");
        engine.setActiveScene(std::move(mainScene));

        const auto skybox = engine.buildCubemap({
            Fate::PathUtils::getEnginePath() / "resources/Textures/Skyboxes/canary_wharf_8k/plusX.jpeg",
            Fate::PathUtils::getEnginePath() / "resources/Textures/Skyboxes/canary_wharf_8k/minusX.jpeg",
            Fate::PathUtils::getEnginePath() / "resources/Textures/Skyboxes/canary_wharf_8k/plusY.jpeg",
            Fate::PathUtils::getEnginePath() / "resources/Textures/Skyboxes/canary_wharf_8k/minusY.jpeg",
            Fate::PathUtils::getEnginePath() / "resources/Textures/Skyboxes/canary_wharf_8k/plusZ.jpeg",
            Fate::PathUtils::getEnginePath() / "resources/Textures/Skyboxes/canary_wharf_8k/minusZ.jpeg",
        });
        engine.getActiveScene()->setSkybox(skybox);

        const auto carModelPath = Fate::PathUtils::getEnginePath() / "resources/Models/mercevo2/1990 Mercedes-Benz 190 Evo II.glb";
        const auto carAsset = engine.buildAssetSceneObject(carModelPath);
        carAsset->setName("Mercedes-Benz 190 Evo II");
        carAsset->getTransform().setPosition({-4.0f, -0.5f, 0.0f});
        engine.getActiveScene()->addObject(*carAsset);

        const auto helmetModelPath = Fate::PathUtils::getEnginePath() / "resources/Models/damagedhelmet/DamagedHelmet.glb";
        const auto helmetAsset = engine.buildAssetSceneObject(helmetModelPath);
        helmetAsset->setName("Damaged Helmet");
        engine.getActiveScene()->addObject(*helmetAsset);

        engine.run();
        return 0;
    }

    return 0;
}
