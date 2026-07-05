#include "Files.h"

#include <array>
#include <format>
#include <memory>
#include <print>
#include <fstream>
#include <vector>

#include "spdlog/spdlog.h"

#define WUFFS_IMPLEMENTATION
#include "wuffs-v0.4.c"

namespace Fate::FileUtils {
    std::string prettyBytes(const std::size_t bytes) {
        constexpr std::array<std::string_view, 5> units = {"B", "KiB", "MiB", "GiB", "TiB"};
        auto size = static_cast<double>(bytes);
        std::size_t unitIndex = 0;

        while (size >= 1024 && unitIndex < units.size() - 1) {
            size /= 1024;
            ++unitIndex;
        }

        return std::format("{:.3} {}", size, units[unitIndex]);
    }

    namespace {
        struct RgbaCallbacks final : wuffs_aux::DecodeImageCallbacks {
            wuffs_base__pixel_format SelectPixfmt(const wuffs_base__image_config&) override {
                return wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL);
            }
        };
    }

    std::unique_ptr<std::uint8_t[]> decodeImage(const std::uint8_t* data, const std::size_t dataSize, std::uint32_t& outWidth, std::uint32_t& outHeight) {
        RgbaCallbacks callbacks;
        wuffs_aux::sync_io::MemoryInput input(data, dataSize);
        wuffs_aux::DecodeImageResult result = wuffs_aux::DecodeImage(callbacks, input);

        if (!result.error_message.empty()) {
            spdlog::error("Failed to decode image: {}", result.error_message);
            return nullptr;
        }

        outWidth = wuffs_base__pixel_config__width(&result.pixbuf.pixcfg);
        outHeight = wuffs_base__pixel_config__height(&result.pixbuf.pixcfg);

        const wuffs_base__table_u8 table = wuffs_base__pixel_buffer__plane(&result.pixbuf, 0);
        auto pixels = std::make_unique_for_overwrite<std::uint8_t[]>(table.width * table.height);

        // todo figure out ownership/lifetimes on Wuffs' own buffer
        for (std::uint32_t y = 0; y < table.height; ++y) {
            std::memcpy(pixels.get() + y * table.width, table.ptr + y * table.stride, table.width);
        }

        return pixels;
    }

    std::unique_ptr<std::uint8_t[]> decodeImageFromPath(const std::filesystem::path& path, std::uint32_t& outWidth, std::uint32_t& outHeight) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            spdlog::error("Failed to open image file: {}", path.string());
            return nullptr;
        }

        const std::size_t fileSize = file.tellg();
        std::vector<std::uint8_t> buffer(fileSize);
        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
        file.close();

        return decodeImage(buffer.data(), fileSize, outWidth, outHeight);
    }
}
