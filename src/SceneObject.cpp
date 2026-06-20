#include "SceneObject.h"

SceneObject::SceneObject(std::string name)
    : transform(glm::mat4(1.0f)), name(std::move(name)) {
}
