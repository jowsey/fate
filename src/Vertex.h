#pragma once
#include <array>

class Vertex {
    std::array<float, 3> position{};
    std::array<float, 3> color{};

public:
    Vertex(std::array<float, 3> position, std::array<float, 3> color);
};
