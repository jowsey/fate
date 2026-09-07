#include "SceneObject.h"

#include <utility>

namespace Fate {
    SceneObject::SceneObject(std::string name)
        : name(std::move(name)), transform(*this) {
    }

    bool SceneObject::getActiveSelf() const {
        return active;
    }

    bool SceneObject::getActiveHierarchy() const {
        if (transform.getParent()) {
            return active && transform.getParent()->getObject().getActiveHierarchy();
        }

        return active;
    }

    void SceneObject::setActive(const bool newValue) {
        this->active = newValue;
    }

    const std::string& SceneObject::getName() const {
        return name;
    }

    void SceneObject::setName(const std::string_view& newValue) {
        this->name = newValue;
    }

    SceneTransform& SceneObject::getTransform() {
        return transform;
    }

    const std::vector<std::shared_ptr<Mesh>>& SceneObject::getMeshes() const {
        return meshes;
    }

    void SceneObject::addMesh(std::shared_ptr<Mesh> mesh) {
        meshes.push_back(std::move(mesh));
    }
}
