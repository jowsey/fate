#include "Scene.h"

#include <algorithm>
#include <print>
#include <utility>

Scene::Scene(std::string name) : name(std::move(name)) {
}

void Scene::addObject(SceneObject&object) {
    if (std::ranges::find(objects, &object) != objects.end()) {
        std::println("Warning: Scene already contains object \"{}\". Skipping.", object.getName());
        return;
    }

    objects.push_back(&object);
}
