#pragma once
#include <string>

#include "MeshHandle.h"
#include "SceneTransform.h"

class SceneObject {
    std::string name;
    SceneTransform transform;
    std::shared_ptr<MeshHandle> meshHandle;

public:
    explicit SceneObject(std::string name, std::shared_ptr<MeshHandle> mesh);

    [[nodiscard]] const std::string& getName() const { return name; }

    [[nodiscard]] SceneTransform& getTransform() { return transform; }

    [[nodiscard]] const std::shared_ptr<MeshHandle>& getMeshHandle() const { return meshHandle; }
};
