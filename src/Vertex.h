#pragma once

namespace Fate {
    struct Vertex {
        std::array<float, 4> baseColour{1.0f, 1.0f, 1.0f, 1.0f};
        std::array<float, 3> position{};
        std::array<float, 3> normal{};
        std::array<float, 4> tangent{};
        std::array<float, 2> texCoord{};
    };
}
