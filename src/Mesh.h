#pragma once
#include <vector>

#include "Vertex.h"

class Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;

public:
    Mesh(std::vector<Vertex> vertices, std::vector<std::uint32_t> indices);

    [[nodiscard]] const std::vector<Vertex>& getVertices() const { return vertices; }
    [[nodiscard]] const std::vector<std::uint32_t>& getIndices() const { return indices; }
};
