#include "FateEngine.h"

#include "Scene.h"

void FateEngine::run() {
    while (!glfwWindowShouldClose(renderer.getWindow())) {
        renderer.render(*activeScene);
    }
}

void FateEngine::setActiveScene(std::unique_ptr<Scene> scene) {
    activeScene = std::move(scene);
}
