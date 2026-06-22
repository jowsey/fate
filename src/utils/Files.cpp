#include "Files.h"

#include <array>
#include <format>
#include <memory>
#include <print>
#include <fstream>
#include <vector>

#define WUFFS_IMPLEMENTATION
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__ADLER32
#define WUFFS_CONFIG__MODULE__CRC32
#define WUFFS_CONFIG__MODULE__DEFLATE
#define WUFFS_CONFIG__MODULE__PNG
#define WUFFS_CONFIG__MODULE__ZLIB
#include "wuffs-v0.3.c"

namespace FileUtils {
    std::string prettyBytes(const std::size_t bytes) {
        constexpr std::array<std::string_view, 5> units = {"B", "KB", "MB", "GB", "TB"};
        auto size = static_cast<double>(bytes);
        std::size_t unitIndex = 0;

        while (size >= 1000 && unitIndex < units.size() - 1) {
            size /= 1000;
            ++unitIndex;
        }

        return std::format("{:.3} {}", size, units[unitIndex]);
    }

    // don't ask
    std::unique_ptr<std::uint8_t[]> decodePng(const uint8_t* data, const size_t dataSize, uint32_t& outWidth, uint32_t& outHeight) {
        auto io = wuffs_base__ptr_u8__reader(const_cast<uint8_t *>(data), dataSize, true);
        io.meta.wi = dataSize;

        const auto dec = std::make_unique<wuffs_png__decoder>();
        auto status = wuffs_png__decoder__initialize(
            dec.get(),
            sizeof(*dec),
            WUFFS_VERSION,
            WUFFS_INITIALIZE__DEFAULT_OPTIONS
        );

        if (!wuffs_base__status__is_ok(&status)) {
            std::println(stderr, "Wuffs initialization failed: {}", wuffs_base__status__message(&status));
            return {};
        }

        wuffs_base__image_config config;
        status = wuffs_png__decoder__decode_image_config(dec.get(), &config, &io);
        if (!wuffs_base__status__is_ok(&status)) {
            std::println(stderr, "failed to decode PNG header: {}", wuffs_base__status__message(&status));
            return {};
        }

        outWidth = wuffs_base__pixel_config__width(&config.pixcfg);
        outHeight = wuffs_base__pixel_config__height(&config.pixcfg);

        wuffs_base__pixel_config__set(
            &config.pixcfg,
            WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL,
            WUFFS_BASE__PIXEL_SUBSAMPLING__NONE,
            outWidth,
            outHeight
        );

        const size_t pixelBufferSize = wuffs_base__pixel_config__pixbuf_len(&config.pixcfg);
        std::unique_ptr<std::uint8_t[]> pixels = std::make_unique_for_overwrite<std::uint8_t[]>(pixelBufferSize);

        wuffs_base__pixel_buffer pixelBuffer;
        status = wuffs_base__pixel_buffer__set_from_slice(&pixelBuffer, &config.pixcfg, wuffs_base__make_slice_u8(pixels.get(), pixelBufferSize));

        if (!wuffs_base__status__is_ok(&status)) {
            std::println(stderr, "failed to configure pixel buffer layout: {}", wuffs_base__status__message(&status));
            return {};
        }

        const size_t workBufferSize = wuffs_png__decoder__workbuf_len(dec.get()).max_incl;
        const std::unique_ptr<unsigned char[]> workBuffer = std::make_unique_for_overwrite<uint8_t[]>(workBufferSize);

        status = wuffs_png__decoder__decode_frame(
            dec.get(),
            &pixelBuffer,
            &io,
            WUFFS_BASE__PIXEL_BLEND__SRC,
            wuffs_base__make_slice_u8(workBuffer.get(), workBufferSize),
            nullptr
        );

        if (!wuffs_base__status__is_ok(&status)) {
            std::println(stderr, "failed to decode PNG image: {}", wuffs_base__status__message(&status));
            return {};
        }

        return pixels;
    }


    std::unique_ptr<std::uint8_t[]> loadPngFromFile(const std::filesystem::path& path, uint32_t& outWidth, uint32_t& outHeight) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            std::println(stderr, "failed to open PNG file: {}", path.string());
            return {};
        }

        const std::vector<uint8_t> fileData((std::istreambuf_iterator(file)), std::istreambuf_iterator<char>());

        auto pixels = decodePng(
            fileData.data(),
            fileData.size(),
            outWidth,
            outHeight
        );

        return pixels;
    }
}
