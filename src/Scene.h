#pragma once

#include "SceneObject.h"

namespace Fate {
    class Scene {
        std::vector<SceneObject *> objects{};
        AllocatedTexture* skyboxCubemap{};
        std::string name;

    public:
        explicit Scene(std::string name);

        [[nodiscard]] const std::string& getName() const { return name; }

        [[nodiscard]] const std::vector<SceneObject *>& getObjects() const { return objects; }

        [[nodiscard]] const AllocatedTexture* getSkybox() const { return skyboxCubemap; }

        void addObject(SceneObject& object);

        void setSkybox(AllocatedTexture* cubemap);
    };
}
