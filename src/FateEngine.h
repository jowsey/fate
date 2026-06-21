#pragma once
#include "FateRenderer.h"

#include "Scene.h"
#include "assimp/scene.h"

class Scene;

class FateEngine {
    FateRenderer renderer;
    std::unique_ptr<Scene> activeScene;

    SceneObject* buildNodeSceneObject(const aiNode* node, const aiScene* scene);

    glm::dvec3 cameraPosition{0};
    glm::quat cameraRotation{1, 0, 0, 0};

public:
    void run();

    void setActiveScene(std::unique_ptr<Scene> scene);

    Scene* getActiveScene() const { return activeScene.get(); }

    SceneObject* buildAssetSceneObject(const std::filesystem::path& path);

    [[nodiscard]] FateRenderer& getRenderer() { return renderer; }
};
