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

        [[nodiscard]] bool getActive() const { return active; }

        void setActive(bool newValue);

        [[nodiscard]] const std::string& getName() const { return name; }

        void setName(const std::string_view& newValue);

        [[nodiscard]] SceneTransform& getTransform() { return transform; }

        [[nodiscard]] const std::vector<std::shared_ptr<Mesh>>& getMeshes() const { return meshes; }

        void addMesh(std::shared_ptr<Mesh> mesh);
    };
}
