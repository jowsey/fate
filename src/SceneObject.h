#pragma once
#include <string>

#include "Mesh.h"
#include "SceneTransform.h"

namespace Fate {
    class SceneObject {
        std::string name;
        SceneTransform transform;

        std::vector<std::shared_ptr<Mesh>> meshes{};

    public:
        explicit SceneObject(std::string name);

        [[nodiscard]] const std::string& getName() const { return name; }

        [[nodiscard]] SceneTransform& getTransform() { return transform; }

        [[nodiscard]] const std::vector<std::shared_ptr<Mesh>>& getMeshes() const { return meshes; }

        void addMesh(std::shared_ptr<Mesh> mesh);
    };
}
