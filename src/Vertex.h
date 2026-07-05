#pragma once

namespace Fate {
    struct Vertex {
        std::array<std::uint8_t, 4> baseColour{255, 255, 255, 255};
        std::array<float, 3> position{};
        std::uint32_t normal{};
        std::uint32_t tangent{};
        std::array<float, 2> texCoord{};
    };
}
