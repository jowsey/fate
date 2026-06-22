#pragma once
#include "FateRenderer.h"

#include "Scene.h"
#include "assimp/scene.h"

class Scene;

class FateEngine {
    FateRenderer renderer;
    std::unique_ptr<Scene> activeScene;

    Material processNodeMaterial(const aiMaterial* nodeMaterial, const aiScene* scene);

    Mesh processNodeMesh(const aiMesh* mesh, const aiScene* scene);

    SceneObject* buildNodeSceneObject(const aiNode* node, const aiScene* scene);

    double lastTime{0};
    double deltaTime{0};

public:
    void run();

    void setActiveScene(std::unique_ptr<Scene> scene);

    Scene* getActiveScene() const { return activeScene.get(); }

    SceneObject* buildAssetSceneObject(const std::filesystem::path& path);

    [[nodiscard]] FateRenderer& getRenderer() { return renderer; }
};
