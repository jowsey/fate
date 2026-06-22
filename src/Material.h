#pragma once
#include <optional>
#include <glad/glad.h>

struct Material {
    // todo this is horrible and needs to die, see Mesh.h
    std::optional<GLuint64> albedoMapHandle;

    float metallic;
    float roughness;
};
