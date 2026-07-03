#pragma once
#include <array>
#include <memory>

namespace Fate {
    struct CubemapData {
        std::uint32_t faceWidth;
        std::uint32_t faceHeight;

        // +X, -X, +Y, -Y, +Z, -Z
        std::array<std::unique_ptr<std::uint8_t[]>, 6> faces;
    };
}
