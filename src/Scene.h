#pragma once

#include "SceneObject.h"

namespace Fate {
    class Scene {
        std::vector<SceneObject *> objects{};
        std::string name;

    public:
        explicit Scene(std::string name);

        [[nodiscard]] const std::string& getName() const { return name; }

        [[nodiscard]] const std::vector<SceneObject *>& getObjects() const { return objects; }

        void addObject(SceneObject& object);
    };
}
