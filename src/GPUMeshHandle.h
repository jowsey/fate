#pragma once

#include "vk_mem_alloc.h"

struct GPUMeshHandle {
    std::uint32_t verticesOffset;
    std::uint32_t indicesOffset;

    VmaVirtualAllocation vertexVirtualAllocation; // todo call vmaVirtualFree on unload, refcount?
    VmaVirtualAllocation indexVirtualAllocation;
};
