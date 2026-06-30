#pragma once

#include <cstdint>

struct GPUMeshHandle {
    std::uint32_t verticesOffset;
    std::uint32_t indicesOffset;
    VmaVirtualAllocation virtualAllocation; // todo call vmaVirtualFree on unload, refcount?
};
