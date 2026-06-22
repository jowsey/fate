#pragma once

#include <filesystem>

namespace PathUtils {
    std::filesystem::path getExecutablePath();

    std::filesystem::path getEnginePath();

    void openBrowser(const std::string& url);
}
