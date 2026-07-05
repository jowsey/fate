#include "Engine.h"

#include <print>

#include "imgui_impl_sdl3.h"
#include "Material.h"
#include "Scene.h"
#include "spdlog/spdlog.h"
#include "assimp/GltfMaterial.h"
#include "assimp/Importer.hpp"
#include "assimp/material.h"
#include "assimp/mesh.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "utils/Paths.h"
#include "utils/Files.h"
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_timer.h"

namespace Fate {
    void Engine::run() {
        bool running = true;
        SDL_Event event; // todo untangle render/engine wrt window

        while (running) {
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL3_ProcessEvent(&event);

                if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                    spdlog::debug("Window resized to {}x{}", event.window.data1, event.window.data2);
                    renderer.updateSwapchain = true;
                }
                else if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                    break;
                }
            }

            const double currentTime = static_cast<double>(SDL_GetPerformanceCounter()) / static_cast<double>(SDL_GetPerformanceFrequency());
            deltaTime = currentTime - lastTime;
            lastTime = currentTime;

            renderer.buildEditorUI(*activeScene, deltaTime);
            renderer.render(*activeScene);
        }
    }

    void Engine::setActiveScene(std::unique_ptr<Scene> scene) {
        activeScene = std::move(scene);
    }

    std::optional<AllocatedTexture *> Engine::tryUploadMaterialTexture(const std::filesystem::path& modelPath, const aiMaterial* nodeMaterial, const aiScene* scene, const aiTextureType textureType, const TextureColourSpace colourSpace) {
        aiString texturePath;
        if (nodeMaterial->GetTexture(textureType, 0, &texturePath) == AI_SUCCESS) {
            spdlog::debug("Material {} has {} texture at {}", nodeMaterial->GetName().C_Str(), aiTextureTypeToString(textureType), texturePath.C_Str());

            // check cache
            std::string cacheKey = modelPath.string() + ":" + texturePath.C_Str();
            if (textureLoaderCache.contains(cacheKey)) {
                spdlog::trace("⤷ Texture found in cache");
                return textureLoaderCache.at(cacheKey);
            }

            if (const aiTexture* texture = scene->GetEmbeddedTexture(texturePath.C_Str())) {
                // is embedded texture
                spdlog::trace("⤷ Texture is embedded");

                if (texture->mHeight == 0) {
                    // is compressed
                    spdlog::trace("⤷ Texture is compressed ({})", texture->achFormatHint);

                    const auto buffer = reinterpret_cast<const uint8_t *>(texture->pcData);
                    const std::size_t bufferSize = texture->mWidth;

                    spdlog::trace("⤷ Texture data has size {}", FileUtils::prettyBytes(bufferSize));

                    std::uint32_t width, height;
                    std::unique_ptr<std::uint8_t[]> decodedData = FileUtils::decodeImage(buffer, bufferSize, width, height);

                    spdlog::trace("⤷ Texture has dimensions {}x{}", width, height);

                    auto textureData = TextureData{width, height, std::move(decodedData)};
                    auto textureHandle = renderer.uploadTexture(textureData);
                    textureLoaderCache[cacheKey] = textureHandle;

                    return textureHandle;
                }

                // is uncompressed
                const auto width = texture->mWidth;
                const auto height = texture->mHeight;
                spdlog::trace("⤷ Texture is uncompressed, dimensions {}x{}", width, height);

                // todo guesswork, test
                const auto pixelData = reinterpret_cast<uint8_t *>(texture->pcData);
                std::unique_ptr<std::uint8_t[]> pixels = std::make_unique_for_overwrite<std::uint8_t[]>(width * height * 4);
                std::memcpy(pixels.get(), pixelData, width * height * 4);

                auto textureData = TextureData{width, height, std::move(pixels)};
                auto textureHandle = renderer.uploadTexture(textureData);
                textureLoaderCache[cacheKey] = textureHandle;

                return textureHandle;
            }

            // is non-embedded texture
            spdlog::trace("⤷ Texture is on disk, path: {}", texturePath.C_Str());

            std::uint32_t width, height;
            std::filesystem::path absolutePath = modelPath.parent_path() / texturePath.C_Str();
            std::unique_ptr<std::uint8_t[]> decodedData = FileUtils::decodeImageFromPath(absolutePath.string(), width, height);

            auto textureData = TextureData{width, height, std::move(decodedData)};
            auto textureHandle = renderer.uploadTexture(textureData);
            textureLoaderCache[cacheKey] = textureHandle;

            return textureHandle;
        }

        return std::nullopt;
    }

    Mesh Engine::processNodeMesh(const std::filesystem::path& modelPath, const aiMesh* mesh, const aiScene* scene) {
        std::vector<Vertex> vertices;
        vertices.reserve(mesh->mNumVertices);
        std::vector<std::uint32_t> indices;
        indices.reserve(mesh->mNumFaces * 3);

        for (std::size_t i = 0; i < mesh->mNumVertices; i++) {
            Vertex vertex;

            if (mesh->HasVertexColors(0)) {
                vertex.baseColour = {mesh->mColors[0][i].r, mesh->mColors[0][i].g, mesh->mColors[0][i].b, mesh->mColors[0][i].a};
            }

            if (mesh->HasPositions()) {
                vertex.position = {mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z};
            }

            if (mesh->HasNormals()) {
                vertex.normal = {mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z};
            }

            if (mesh->HasTangentsAndBitangents()) {
                const float handedness = (mesh->mNormals[i] ^ mesh->mTangents[i]) * mesh->mBitangents[i] > 0.0f ? 1.0f : -1.0f;
                vertex.tangent = {mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z, handedness};
            }

            if (mesh->HasTextureCoords(0)) {
                vertex.texCoord = {mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y};
            }

            vertices.push_back(vertex);
        }

        for (std::size_t i = 0; i < mesh->mNumFaces; i++) {
            const aiFace face = mesh->mFaces[i];
            for (std::size_t j = 0; j < face.mNumIndices; j++) {
                indices.push_back(face.mIndices[j]);
            }
        }

        // Process material
        const aiMaterial* nodeMaterial = scene->mMaterials[mesh->mMaterialIndex];

        aiString materialName;
        if (nodeMaterial->Get(AI_MATKEY_NAME, materialName) == AI_SUCCESS) {
            std::string nameStr = materialName.C_Str();
            spdlog::debug("Processing material: {}", materialName.C_Str());
        }

        Material material;
        nodeMaterial->Get(AI_MATKEY_BASE_COLOR, material.baseColour);
        nodeMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, material.roughness);
        nodeMaterial->Get(AI_MATKEY_METALLIC_FACTOR, material.metallic);

        aiString alphaMode;
        if (nodeMaterial->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == AI_SUCCESS) {
            const std::string modeStr = alphaMode.C_Str();
            material.useAlpha = modeStr != "OPAQUE";

            // hack: re-route transmission extension to flat opacity, todo
            if (!material.useAlpha) {
                float transmissionFactor;
                const bool isTransmission = nodeMaterial->Get(AI_MATKEY_TRANSMISSION_FACTOR, transmissionFactor) == AI_SUCCESS;

                if (isTransmission) {
                    material.useAlpha = true;
                    material.baseColour.a = 0.01f;
                }
            }
        }

        // todo deduplicate
        if (auto albedoMap = tryUploadMaterialTexture(modelPath, nodeMaterial, scene, aiTextureType_DIFFUSE, TextureColourSpace::SRGB)) {
            material.albedoMap = *albedoMap;
            material.mapFlags |= static_cast<std::uint32_t>(MapFlags::HasAlbedoMap);
        }

        if (auto normalMap = tryUploadMaterialTexture(modelPath, nodeMaterial, scene, aiTextureType_NORMALS, TextureColourSpace::Linear)) {
            material.normalMap = *normalMap;
            material.mapFlags |= static_cast<std::uint32_t>(MapFlags::HasNormalMap);
        }

        if (auto ambientMap = tryUploadMaterialTexture(modelPath, nodeMaterial, scene, aiTextureType_LIGHTMAP, TextureColourSpace::Linear)) {
            material.ambientMap = *ambientMap;
            material.mapFlags |= static_cast<std::uint32_t>(MapFlags::HasAmbientMap);
        }

        if (auto roughnessMap = tryUploadMaterialTexture(modelPath, nodeMaterial, scene, aiTextureType_DIFFUSE_ROUGHNESS, TextureColourSpace::Linear)) {
            material.roughnessMap = *roughnessMap;
            material.mapFlags |= static_cast<std::uint32_t>(MapFlags::HasRoughnessMap);
        }

        if (auto metallicMap = tryUploadMaterialTexture(modelPath, nodeMaterial, scene, aiTextureType_METALNESS, TextureColourSpace::Linear)) {
            material.metallicMap = *metallicMap;
            material.mapFlags |= static_cast<std::uint32_t>(MapFlags::HasMetallicMap);
        }

        if (auto emissiveMap = tryUploadMaterialTexture(modelPath, nodeMaterial, scene, aiTextureType_EMISSIVE, TextureColourSpace::SRGB)) {
            material.emissiveMap = *emissiveMap;
            material.mapFlags |= static_cast<std::uint32_t>(MapFlags::HasEmissiveMap);
        }

        auto newMesh = Mesh(std::move(vertices), std::move(indices), std::make_shared<Material>(material));
        return newMesh;
    }

    SceneObject* Engine::buildNodeSceneObject(const std::filesystem::path& modelPath, const aiNode* node, const aiScene* scene) {
        const auto sceneObject = new SceneObject(node->mName.C_Str());

        aiVector3D position;
        aiQuaternion rotation;
        aiVector3D scale;

        node->mTransformation.Decompose(scale, rotation, position);

        sceneObject->getTransform().setPosition({position.x, position.y, position.z});
        sceneObject->getTransform().setRotation({rotation.w, rotation.x, rotation.y, rotation.z});
        sceneObject->getTransform().setLocalScale({scale.x, scale.y, scale.z});

        for (std::size_t i = 0; i < node->mNumMeshes; i++) {
            const aiMesh* nodeMesh = scene->mMeshes[node->mMeshes[i]];

            auto objectMesh = std::make_shared<Mesh>(processNodeMesh(modelPath, nodeMesh, scene));
            // todo renderer should handle this as required, engine shouldn't know about GPU handles
            objectMesh->setGPUHandle(renderer.uploadMesh(*objectMesh));

            sceneObject->addMesh(std::move(objectMesh));
        }

        for (std::size_t i = 0; i < node->mNumChildren; i++) {
            SceneObject* childObject = buildNodeSceneObject(modelPath, node->mChildren[i], scene);
            childObject->getTransform().setParent(sceneObject->getTransform());
        }

        return sceneObject;
    }

    SceneObject* Engine::buildAssetSceneObject(const std::filesystem::path& path) {
        static Assimp::Importer importer;

        spdlog::debug("Building asset tree for {}", path.lexically_normal().string());

        const aiScene* scene = importer.ReadFile(
            path.string(),
            aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices |
            aiProcess_FlipUVs |
            aiProcess_GenSmoothNormals |
            aiProcess_CalcTangentSpace
        );

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            spdlog::error("Error while building asset: {}", importer.GetErrorString());
            return nullptr;
        }

        return buildNodeSceneObject(path, scene->mRootNode, scene);
    }

    AllocatedTexture* Engine::buildCubemap(const std::array<std::filesystem::path, 6>& facePaths) {
        CubemapData data{};

        for (std::size_t i = 0; i < 6; ++i) {
            const auto& path = facePaths[i];
            spdlog::debug("Loading cubemap face {} from {}", i, path.lexically_normal().string());

            std::unique_ptr<std::uint8_t[]> pixels = FileUtils::decodeImageFromPath(path, data.faceWidth, data.faceHeight);
            if (!pixels) {
                spdlog::error("Failed to load cubemap face from path: {}", path.lexically_normal().string());
                return nullptr;
            }

            data.faces[i] = std::move(pixels);
        }

        return renderer.uploadCubemap(data);
    }
}
