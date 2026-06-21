#include "Mesh.h"

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<std::uint32_t> indices)
    : vertices(std::move(vertices)), indices(std::move(indices)) {
}
