#include "FateRenderer.h"

int main(const int argc, char** argv) {
    const FateRenderer renderer;

    while (!glfwWindowShouldClose(renderer.getWindow())) {
        renderer.render();
    }

    return 0;
}
