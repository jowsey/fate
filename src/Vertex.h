#pragma once
#include "glm/glm.hpp"

struct Vertex {
    glm::vec4 baseColour{1.0f};
    glm::vec3 position{};
    glm::vec3 normal{};
    glm::vec2 texCoord{};
};
