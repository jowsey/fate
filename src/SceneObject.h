#pragma once
#include <string>

#include "Mesh.h"
#include "MeshHandle.h"
#include "SceneTransform.h"

class SceneObject {
    std::string name;
    SceneTransform transform;

    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<MeshHandle> meshHandle; // todo combine?

public:
    explicit SceneObject(std::string name, std::shared_ptr<Mesh> mesh, std::shared_ptr<MeshHandle> meshHandle);

    [[nodiscard]] const std::string& getName() const { return name; }

    [[nodiscard]] SceneTransform& getTransform() { return transform; }

    [[nodiscard]] const std::shared_ptr<Mesh>& getMesh() const { return mesh; }

    [[nodiscard]] const std::shared_ptr<MeshHandle>& getMeshHandle() const { return meshHandle; }
};
