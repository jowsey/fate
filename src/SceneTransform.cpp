#include "SceneTransform.h"

#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Fate {
    void SceneTransform::recomputeWorldMatrix() {
        // todo all this casting obviously sucks, improve
        worldMatrix = glm::translate(glm::dmat4(1.0f), localPosition) *
                      glm::dmat4(glm::mat4(localRotation)) *
                      glm::scale(glm::dmat4(1.0f), glm::dvec3(localScale));

        if (parent) {
            worldMatrix = parent->getWorldMatrix() * worldMatrix;
        }

        isDirty = false;
    }

    void SceneTransform::setDirty() {
        for (const auto child: children) {
            child->setDirty();
        }

        isDirty = true;
    }

    SceneTransform::SceneTransform(SceneObject& object) : object(&object) {
    }

    void SceneTransform::setParent(SceneTransform& newParent) {
        if (parent) {
            const auto childIt = std::ranges::find(parent->children, this);
            parent->children.erase(childIt);
        }

        parent = &newParent;
        setDirty();

        parent->children.push_back(this);
    }

    const glm::dmat4& SceneTransform::getWorldMatrix() {
        if (isDirty) {
            recomputeWorldMatrix();
        }

        return worldMatrix;
    }

    void SceneTransform::setPosition(const glm::dvec3& pos) {
        localPosition = pos;
        setDirty();
    }

    void SceneTransform::setRotation(const glm::quat& rot) {
        localRotation = rot;
        localEulerAngles = glm::degrees(glm::eulerAngles(rot));
        setDirty();
    }

    void SceneTransform::setEulerAngles(const glm::vec3& rot) {
        localEulerAngles = rot;
        localRotation = glm::quat(glm::radians(rot));
        setDirty();
    }

    void SceneTransform::setLocalScale(const glm::vec3& scale) {
        localScale = scale;
        setDirty();
    }
}
