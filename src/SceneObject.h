#pragma once
#include <string>

#include "Mesh.h"
#include "SceneTransform.h"

namespace Fate {
    class SceneObject {
        bool active{true};
        std::string name;

        SceneTransform transform;

        std::vector<std::shared_ptr<Mesh>> meshes{};

    public:
        explicit SceneObject(std::string name);

        [[nodiscard]] bool getActiveSelf() const;

        [[nodiscard]] bool getActiveHierarchy() const;

        void setActive(bool newValue);

        [[nodiscard]] const std::string& getName() const;

        void setName(const std::string_view& newValue);

        [[nodiscard]] SceneTransform& getTransform();

        [[nodiscard]] const std::vector<std::shared_ptr<Mesh>>& getMeshes() const;

        void addMesh(std::shared_ptr<Mesh> mesh);
    };
}
