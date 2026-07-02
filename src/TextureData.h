#pragma once
#include <cstdint>

namespace Fate {
    struct TextureData {
        std::uint32_t width;
        std::uint32_t height;

        std::unique_ptr<std::uint8_t[]> pixels;
    };
}
