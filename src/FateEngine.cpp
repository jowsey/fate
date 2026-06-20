#include "FateEngine.h"

void FateEngine::run() const {
    while (!glfwWindowShouldClose(renderer.getWindow())) {
        renderer.render();
    }
}
