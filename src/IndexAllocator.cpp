#include "IndexAllocator.h"

namespace Fate {
    IndexAllocator::IndexAllocator(const std::uint32_t numIndices)
        : maxIndex(numIndices) {
    }

    std::uint32_t IndexAllocator::getNext() {
        if (!freedIndices.empty()) {
            const auto index = freedIndices.front();
            freedIndices.pop();
            return index;
        }

        if (nextFreeIndex < maxIndex) {
            return nextFreeIndex++;
        }

        // todo do something about it?
        throw std::runtime_error("Requested index from IndexAllocator but none were available");
    }

    void IndexAllocator::free(const std::uint32_t index) {
        if (index >= nextFreeIndex) {
            throw std::runtime_error("Cannot free index that has not been allocated");
        }
        // todo can we check if in queue? maybe we give classes that store whether they've been freed?

        freedIndices.push(index);
    }
}
