#pragma once
#include <filesystem>
#include <map>

#include "Renderer.h"

#include "Scene.h"
#include "assimp/scene.h"

class Scene;

namespace Fate {
    class Engine {
        Renderer renderer;
        std::unique_ptr<Scene> activeScene;

        std::optional<AllocatedTexture*> tryUploadMaterialTexture(const std::filesystem::path& modelPath, const aiMaterial* nodeMaterial, const aiScene* scene, aiTextureType textureType, TextureColourSpace colourSpace);

        Mesh processNodeMesh(const std::filesystem::path& modelPath, const aiMesh* mesh, const aiScene* scene);

        SceneObject* buildNodeSceneObject(const std::filesystem::path& modelPath, const aiNode* node, const aiScene* scene);

        double lastTime{0};
        double deltaTime{0};

        std::map<std::string, AllocatedTexture*> textureLoaderCache{};

    public:
        void run();

        [[nodiscard]] Renderer& getRenderer() { return renderer; }

        [[nodiscard]] Scene* getActiveScene() const { return activeScene.get(); }

        void setActiveScene(std::unique_ptr<Scene> scene);

        SceneObject* buildAssetSceneObject(const std::filesystem::path& path);

        AllocatedTexture* buildCubemap(const std::array<std::filesystem::path, 6>& facePaths);
    };
}
