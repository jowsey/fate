#include <filesystem>

#include "SDL3/SDL_filesystem.h"

namespace Fate::PathUtils {
    std::filesystem::path getEnginePath() {
        return std::filesystem::path(SDL_GetBasePath()).parent_path().parent_path();
    }
}
