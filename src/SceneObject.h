#pragma once
#include <string>

#include <glm/glm.hpp>

class SceneObject {
    glm::mat4 transform;
    std::string name;

public:
    explicit SceneObject(std::string name);

    [[nodiscard]] const glm::mat4& getTransform() const { return transform; }
};
