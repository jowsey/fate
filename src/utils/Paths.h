#pragma once

#include <filesystem>

namespace Fate::PathUtils {
    std::filesystem::path getEnginePath();

    void openBrowser(const std::string& url);
}
