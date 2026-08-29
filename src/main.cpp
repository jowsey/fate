#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include <CLI/CLI.hpp>

#include "spdlog/spdlog.h"

#include "Engine.h"
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
    initCmd->add_option("name", "Project name")->required();

    try {
        CLI11_PARSE(app, argc, argv);
    }
    catch (const CLI::ParseError& e) {
        return app.exit(e);
    }

    if (initCmd->parsed()) {
        const auto name = initCmd->get_option("name")->as<std::string>();
        const auto relativePath = std::filesystem::path(name);
        const auto absolutePath = std::filesystem::absolute(relativePath);

        if (std::filesystem::exists(absolutePath) && !std::filesystem::is_empty(absolutePath)) {
            std::cerr << "Cannot create project at " << absolutePath.string() << ": directory is not empty." << std::endl;
            return 1;
        }

        std::cout << "Initializing project '" << name << "' at " << absolutePath.string() << std::endl;

        // Build project directory
        if (!std::filesystem::exists(absolutePath)) {
            std::filesystem::create_directories(absolutePath);
        }

        std::filesystem::copy(Fate::PathUtils::getEnginePath() / "resources/ProjectTemplates/Default", absolutePath, std::filesystem::copy_options::recursive);

        // Replace placeholder tags
        const std::array<std::pair<std::string, std::string>, 1> TagReplacements = {
            std::make_pair("{{ProjectName}}", name),
        };

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

        return 0;
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

    const auto helmetModelPath = Fate::PathUtils::getEnginePath() / "resources/Models/damagedhelmet/damagedhelmet.glb";
    const auto helmetAsset = engine.buildAssetSceneObject(helmetModelPath);
    helmetAsset->setName("Damaged Helmet");
    engine.getActiveScene()->addObject(*helmetAsset);

    engine.run();
    return 0;
}
