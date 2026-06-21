#include <ostream>

#include "FateEngine.h"
#include "Scene.h"
#include "assimp/Importer.hpp"
#include "util/Utils.h"

int main(const int argc, char** argv) {
    FateEngine engine;

    auto mainScene = std::make_unique<Scene>("Main");
    engine.setActiveScene(std::move(mainScene));

    const auto carModelPath = getEnginePath() / "resources/Models/mercevo2/1990 Mercedes-Benz 190 Evo II.glb";
    const auto carAsset = engine.buildAssetSceneObject(carModelPath);
    engine.getActiveScene()->addObject(*carAsset);

#pragma region primitives
    // const auto triangleMesh = std::make_shared<Mesh>(
    //     std::vector<Vertex>{
    //         Vertex({0.0f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}),
    //         Vertex({-0.5f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}),
    //         Vertex({0.5f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}),
    //     },
    //     std::vector<std::uint32_t>{0, 1, 2}
    // );
    //
    // const auto cubeMesh = std::make_shared<Mesh>(
    //     std::vector<Vertex>{
    //         Vertex({-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}),
    //         Vertex({0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}),
    //         Vertex({0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}),
    //         Vertex({-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}),
    //         Vertex({-0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}),
    //         Vertex({0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}),
    //         Vertex({0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}),
    //         Vertex({-0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}),
    //     },
    //     std::vector<std::uint32_t>{
    //         0, 1, 2, 2, 3, 0, // front
    //         4, 5, 6, 6, 7, 4, // back
    //         0, 1, 5, 5, 4, 0, // bottom
    //         2, 3, 7, 7, 6, 2, // top
    //         0, 3, 7, 7, 4, 0, // left
    //         1, 2, 6, 6, 5, 1 // right
    //     }
    // );
    //
    // const auto triangleMeshHandle = std::make_shared<MeshHandle>(
    //     engine.getRenderer().uploadMesh(triangleMesh->getVertices(), triangleMesh->getIndices())
    // );
    //
    // const auto cubeMeshHandle = std::make_shared<MeshHandle>(
    //     engine.getRenderer().uploadMesh(cubeMesh->getVertices(), cubeMesh->getIndices())
    // );
    //
    // auto triangle = SceneObject("Triangle", triangleMesh, triangleMeshHandle);
    //
    // auto triangle2 = SceneObject("Triangle2", triangleMesh, triangleMeshHandle);
    // triangle2.getTransform().setPosition({0.5f, -0.5f, 0});
    // triangle2.getTransform().setRotation(glm::angleAxis(glm::radians(-90.0f), glm::vec3(0, 0, 1)));
    //
    // auto cube = SceneObject("Cube", cubeMesh, cubeMeshHandle);
    // cube.getTransform().setPosition({0.0f, -1.0f, 0});
    //
    // mainScene->addObject(triangle);
    // mainScene->addObject(triangle2);
    // mainScene->addObject(cube);
#pragma endregion

    engine.run();
    return 0;
}
