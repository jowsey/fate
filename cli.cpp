#include <iostream>
#include <array>
#include <CLI/CLI.hpp>

#include "src/util/Utils.h"

int main(const int argc, char** argv) {
    CLI::App app;

    app.name("fate");
    app.description("The Fate game engine, v" FATE_VERSION);
    app.footer("Learn more: https://github.com/jowsey/fate");
    app.require_subcommand(1);
    app.set_version_flag("-v,--version", FATE_VERSION);

    CLI::App* initCmd = app.add_subcommand("init", "Initialize a new project");
    initCmd->add_option("name", "Project name")->required();

    CLI11_PARSE(app, argc, argv);

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

        std::filesystem::path engineDir = getExecutablePath().parent_path().parent_path();
        std::filesystem::copy(engineDir / "resources/ProjectTemplates/Default", absolutePath, std::filesystem::copy_options::recursive);

        // Replace placeholder tags
        const std::array<std::pair<std::string, std::string>, 1> TagReplacements = {
            std::make_pair("{{ProjectName}}", name),
        };

        for (const auto&entry: std::filesystem::recursive_directory_iterator(absolutePath)) {
            if (!entry.is_regular_file()) continue;

            const std::filesystem::path&filePath = entry.path();

            // Find in file content
            if (std::ifstream inFile{filePath, std::ios::binary | std::ios::ate}) {
                std::string content(inFile.tellg(), '\0');
                inFile.seekg(0);
                inFile.read(content.data(), content.size());
                inFile.close();

                for (const auto&[tag, replacement]: TagReplacements) {
                    if (const std::size_t contentPos = content.find(tag); contentPos != std::string::npos) {
                        content.replace(contentPos, tag.length(), replacement);

                        if (std::ofstream outFile{filePath, std::ios::binary}) {
                            outFile.write(content.data(), content.size());
                        }
                    }
                }
            }

            // Find in file name
            std::string filename = filePath.filename().string();

            for (const auto&[tag, replacement]: TagReplacements) {
                const std::size_t pos = filename.find(tag);
                if (pos != std::string::npos) {
                    filename.replace(pos, tag.length(), replacement);
                    std::filesystem::path newFilePath = filePath.parent_path() / filename;
                    std::filesystem::rename(filePath, newFilePath);
                }
            }
        }
    }

    return 0;
}
