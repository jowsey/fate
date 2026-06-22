#pragma once
#include <vector>
#include <memory>

#include "Vertex.h"
#include "GPUMeshHandle.h"
#include "Material.h"
#include "assimp/mesh.h"

class Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;

    // todo definitely should be refactored to store in the renderer
    std::optional<GPUMeshHandle> gpuHandle;

    std::shared_ptr<Material> material;

public:
    explicit Mesh(std::vector<Vertex> vertices, std::vector<std::uint32_t> indices, std::shared_ptr<Material> material)
        : vertices(std::move(vertices)), indices(std::move(indices)), material(std::move(material)) {
    }

    [[nodiscard]] const std::vector<Vertex>& getVertices() const { return vertices; }
    [[nodiscard]] const std::vector<std::uint32_t>& getIndices() const { return indices; }

    [[nodiscard]] const std::optional<GPUMeshHandle>& getGPUHandle() const { return gpuHandle; }

    void setGPUHandle(GPUMeshHandle handle) { gpuHandle = handle; }

    [[nodiscard]] const std::shared_ptr<Material>& getMaterial() const { return material; }
};
