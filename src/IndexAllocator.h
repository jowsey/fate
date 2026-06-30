#pragma once
#include <cstdint>
#include <queue>

class IndexAllocator {
    std::uint32_t maxIndex;
    std::uint32_t nextFreeIndex{0};
    std::queue<std::uint32_t> freedIndices{};

public:
    explicit IndexAllocator(std::uint32_t numIndices);

    std::uint32_t getNext();

    void free(const std::uint32_t index);
};
