#include "FateEngine.h"
#include "Scene.h"
#include "glm/ext/quaternion_trigonometric.hpp"

int main(const int argc, char** argv) {
    FateEngine engine;
    const auto triangleMeshHandle = std::make_shared<MeshHandle>(
        engine.getRenderer().uploadMesh(
            {
                Vertex({0.0f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}),
                Vertex({-0.5f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}),
                Vertex({0.5f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}),
            },
            {0, 1, 2}
        )
    );

    auto mainScene = std::make_unique<Scene>("Main");

    auto triangle = SceneObject("Triangle", triangleMeshHandle);
    auto triangle2 = SceneObject("Triangle2", triangleMeshHandle);

    triangle2.getTransform().setPosition({0.5f, -0.5f, 0});
    triangle2.getTransform().setRotation(glm::angleAxis(glm::radians(-90.0f), glm::vec3(0, 0, 1)));

    mainScene->addObject(triangle);
    mainScene->addObject(triangle2);

    engine.setActiveScene(std::move(mainScene));
    engine.run();

    return 0;
}
