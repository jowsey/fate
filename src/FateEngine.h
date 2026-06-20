#pragma once
#include "FateRenderer.h"

#include "Scene.h"

class Scene;

class FateEngine {
    FateRenderer renderer;
    std::unique_ptr<Scene> activeScene;

public:
    void run() const;

    void setActiveScene(std::unique_ptr<Scene> scene);

    [[nodiscard]] FateRenderer& getRenderer() { return renderer; }
};
