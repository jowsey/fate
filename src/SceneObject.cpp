#include "SceneObject.h"

#include <utility>

namespace Fate {
    SceneObject::SceneObject(std::string name)
        : name(std::move(name)), transform(*this) {
    }

    void SceneObject::addMesh(std::shared_ptr<Mesh> mesh) {
        meshes.push_back(std::move(mesh));
    }

    void SceneObject::setActive(const bool newValue) {
        this->active = newValue;
    }

    void SceneObject::setName(const std::string_view& newValue) {
        this->name = newValue;
    }
}
