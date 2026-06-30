#pragma once

#include <cstdint>
#include "vk_mem_alloc.h"

struct GPUMeshHandle {
    std::uint32_t verticesOffset;
    std::uint32_t indicesOffset;
    VmaVirtualAllocation virtualAllocation; // todo call vmaVirtualFree on unload, refcount?
};
