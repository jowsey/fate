#pragma once

#include <filesystem>

std::filesystem::path getExecutablePath();

void openBrowser(const std::string&url);

std::string prettyBytes(std::size_t bytes);
