#pragma once
#include "glm/vec4.hpp"

namespace Fate {
    struct AllocatedTexture;

    enum class MapFlags : std::uint32_t {
        None = 0,
        HasAlbedoMap = 1 << 0,
        HasNormalMap = 1 << 1,
        HasMetallicMap = 1 << 2,
        HasRoughnessMap = 1 << 3,
        HasEmissiveMap = 1 << 4
    };

    struct Material {
        glm::vec4 baseColour{1.0f};
        float metallic;
        float roughness;

        // todo this is horrible and needs to die, see Mesh.h
        AllocatedTexture* albedoMap{};
        AllocatedTexture* normalMap{};
        AllocatedTexture* metallicMap{};
        AllocatedTexture* roughnessMap{};
        AllocatedTexture* emissiveMap{};

        std::uint32_t mapFlags{static_cast<std::uint32_t>(MapFlags::None)};
        bool useAlpha{false};
    };
}
