#pragma once
#include <cstdint>

namespace Fate {
    enum class TextureColourSpace {
        SRGB,
        Linear
    };

    struct TextureData {
        std::uint32_t width;
        std::uint32_t height;

        std::unique_ptr<std::uint8_t[]> pixels;
        TextureColourSpace colourSpace;
    };
}
