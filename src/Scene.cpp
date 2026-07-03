#include "Scene.h"

#include <algorithm>
#include <utility>

#include "spdlog/spdlog.h"

namespace Fate {
    Scene::Scene(std::string name) : name(std::move(name)) {
    }

    void Scene::addObject(SceneObject& object) {
        if (std::ranges::find(objects, &object) != objects.end()) {
            spdlog::warn("Scene already contains object \"{}\". Skipping.", object.getName());
            return;
        }

        objects.push_back(&object);

        for (const auto child: object.getTransform().getChildren()) {
            addObject(child->getObject());
        }
    }

    void Scene::setSkybox(AllocatedTexture* cubemap) {
        skyboxCubemap = cubemap;
    }
}
