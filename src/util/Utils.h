#pragma once

#include <filesystem>

std::filesystem::path getExecutablePath();

std::filesystem::path getEnginePath();

void openBrowser(const std::string& url);

std::string prettyBytes(std::size_t bytes);
