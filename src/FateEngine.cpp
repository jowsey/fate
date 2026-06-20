#include "FateEngine.h"

#include "Scene.h"

void FateEngine::run() const {
    while (!glfwWindowShouldClose(renderer.getWindow())) {
        renderer.render(*activeScene);
    }
}

void FateEngine::setActiveScene(std::unique_ptr<Scene> scene) {
    activeScene = std::move(scene);
}
