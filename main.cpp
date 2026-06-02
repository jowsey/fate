#include <iostream>
#include <CLI/CLI.hpp>

int main(const int argc, char** argv) {
    CLI::App app;

    app.name("velvet");
    app.description("The Velvet game engine");
    app.footer("Learn more: https://github.com/placeholder/velvet");

    CLI::App* initCmd = app.add_subcommand("init", "Initialize a new project");

    initCmd->add_option("name", "Project name")->required();

    CLI11_PARSE(app, argc, argv);

    if (initCmd->parsed()) {
        const std::string name = initCmd->get_option("name")->as<std::string>();
        const auto relativePath = std::filesystem::path(name);
        const auto absolutePath = std::filesystem::absolute(relativePath);

        if (std::filesystem::exists(absolutePath) && !std::filesystem::is_empty(absolutePath)) {
            std::cerr << "Cannot create project at " << absolutePath.string() << ": directory is not empty." << std::endl;
            return 1;
        }

        std::cout << "Initializing project '" << name << "' at " << absolutePath.string() << std::endl;

        if (!std::filesystem::exists(absolutePath)) {
            std::filesystem::create_directories(absolutePath);
        }

        std::filesystem::create_directories(absolutePath / "Assets");
        std::filesystem::create_directories(absolutePath / "ProjectSettings");
    }

    return 0;
}
