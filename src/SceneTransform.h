#pragma once

#include <vector>

#include <glm/glm.hpp>
#include "glm/detail/type_quat.hpp"

namespace Fate {
    class SceneObject;

    class SceneTransform {
        glm::dvec3 localPosition{0};
        glm::quat localRotation{1, 0, 0, 0};
        glm::vec3 localScale{1};

        glm::dmat4 worldMatrix{};

        bool isDirty{true};

        void recomputeWorldMatrix();

        SceneObject* object;

        SceneTransform* parent{};
        std::vector<SceneTransform *> children{};

        void setDirty();

    public:
        explicit SceneTransform(SceneObject& object);

        [[nodiscard]] const glm::dvec3& getPosition() const { return localPosition; }
        [[nodiscard]] const glm::quat& getRotation() const { return localRotation; }
        [[nodiscard]] const glm::vec3& getLocalScale() const { return localScale; }

        [[nodiscard]] const glm::dmat4& getWorldMatrix();

        [[nodiscard]] SceneTransform* getParent() const { return parent; }
        [[nodiscard]] SceneObject& getObject() const { return *object; }
        [[nodiscard]] const std::vector<SceneTransform *>& getChildren() const { return children; }

        void setPosition(const glm::dvec3& pos);

        void setRotation(const glm::quat& rot);

        void setLocalScale(const glm::vec3& scale);

        void setParent(SceneTransform& newParent);
    };
}
