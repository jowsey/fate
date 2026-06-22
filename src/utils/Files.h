#pragma once
#include <filesystem>
#include <memory>
#include <string>

namespace FileUtils {
    std::string prettyBytes(std::size_t bytes);

    std::unique_ptr<std::uint8_t[]> decodePng(const uint8_t* data, const size_t dataSize, uint32_t& outWidth, uint32_t& outHeight);

    std::unique_ptr<std::uint8_t[]> loadPngFromFile(const std::filesystem::path& path, uint32_t& outWidth, uint32_t& outHeight);
}
