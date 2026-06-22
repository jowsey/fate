#pragma once
#include <optional>
#include <glad/glad.h>

#include "glm/vec4.hpp"

enum class MapFlags : std::uint32_t {
    None = 0,
    HasAlbedoMap = 1 << 0,
};

struct Material {
    glm::vec4 baseColour{1.0f};
    float metallic;
    float roughness;

    // todo this is horrible and needs to die, see Mesh.h
    std::optional<GLuint64> albedoMapHandle;

    std::uint32_t mapFlags{static_cast<std::uint32_t>(MapFlags::None)};
    bool useAlpha{false};
};
