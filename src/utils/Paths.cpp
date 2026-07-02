#include <filesystem>
#include <string>

#include "SDL3/SDL_filesystem.h"

namespace Fate::PathUtils {
    std::filesystem::path getEnginePath() {
        return std::filesystem::path(SDL_GetBasePath()).parent_path().parent_path();
    }

    void openBrowser(const std::string& url) {
        std::string command;

#if defined(_WIN32)
        command = "start " + url;
#elif defined(__linux__)
        command = "xdg-open " + url;
#else
        return;
#endif

        std::system(command.c_str());
    }
}
