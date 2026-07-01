#pragma once
#include <filesystem>
#include <memory>
#include <string>

namespace Fate::FileUtils {
    std::string prettyBytes(std::size_t bytes);

    std::unique_ptr<std::uint8_t[]> decodePng(const std::uint8_t* data, std::size_t dataSize, std::uint32_t& outWidth, std::uint32_t& outHeight);

    std::unique_ptr<std::uint8_t[]> decodeJpeg(const std::uint8_t* data, std::size_t dataSize, std::uint32_t& outWidth, std::uint32_t& outHeight);
}
