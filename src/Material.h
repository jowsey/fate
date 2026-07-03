#pragma once
#include "glm/vec4.hpp"

namespace Fate {
    struct AllocatedTexture;

    enum class MapFlags : std::uint32_t {
        None = 0,
        HasAlbedoMap = 1 << 0,
        HasNormalMap = 1 << 1,
        HasAmbientMap = 1 << 2,
        HasRoughnessMap = 1 << 3,
        HasMetallicMap = 1 << 4,
        HasEmissiveMap = 1 << 5
    };

    struct Material {
        glm::vec4 baseColour{1.0f};
        float roughness;
        float metallic;

        // todo this is horrible and needs to die, see Mesh.h
        AllocatedTexture* albedoMap{};
        AllocatedTexture* normalMap{};
        AllocatedTexture* ambientMap{};
        AllocatedTexture* roughnessMap{};
        AllocatedTexture* metallicMap{};
        AllocatedTexture* emissiveMap{};

        std::uint32_t mapFlags{static_cast<std::uint32_t>(MapFlags::None)};
        bool useAlpha{false};
    };
}
