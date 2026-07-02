#include "Engine.h"

#include <print>
#include <iostream>

#include "imgui_impl_sdl3.h"
#include "Material.h"
#include "Scene.h"
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

    std::optional<TextureData> Engine::pullTextureFromMaterial(const aiMaterial* nodeMaterial, const aiScene* scene, const aiTextureType textureType) {
        aiString texturePath;
        if (nodeMaterial->GetTexture(textureType, 0, &texturePath) == AI_SUCCESS) {
            std::print("- Found {} texture at {}", aiTextureTypeToString(textureType), texturePath.C_Str());

            if (const aiTexture* texture = scene->GetEmbeddedTexture(texturePath.C_Str())) {
                // is embedded texture
                std::print(", embedded");

                if (texture->mHeight == 0) {
                    // is compressed texture
                    const std::string_view format = texture->achFormatHint;
                    std::print(", compressed ({})", format);

                    const auto buffer = reinterpret_cast<const uint8_t *>(texture->pcData);
                    const std::size_t bufferSize = texture->mWidth;

                    std::print(", size {}", FileUtils::prettyBytes(bufferSize));

                    std::unique_ptr<std::uint8_t[]> decodedData;

                    std::uint32_t width, height;
                    if (format == "png") {
                        decodedData = FileUtils::decodePng(buffer, bufferSize, width, height);
                    }
                    else if (format == "jpg") {
                        decodedData = FileUtils::decodeJpeg(buffer, bufferSize, width, height);
                    }
                    else {
                        std::println(", unsupported format", format);
                        return std::nullopt;
                    }

                    std::print(", dimensions {}x{}\n", width, height);

                    return TextureData{width, height, std::move(decodedData)};
                }
                else {
                    // is uncompressed texture
                    const auto width = texture->mWidth;
                    const auto height = texture->mHeight;
                    std::print(", uncompressed, dimensions ({}x{})\n", width, height);

                    // todo guesswork, test
                    const auto pixelData = reinterpret_cast<uint8_t *>(texture->pcData);
                    std::unique_ptr<std::uint8_t[]> pixels = std::make_unique_for_overwrite<std::uint8_t[]>(width * height * 4);
                    std::memcpy(pixels.get(), pixelData, width * height * 4);
                    return TextureData{width, height, std::move(pixels)};
                }
            }
        }

        // todo non-embedded textures
        return std::nullopt;
    }

    Material Engine::processNodeMaterial(const aiMaterial* nodeMaterial, const aiScene* scene) {
        aiString materialName;
        if (nodeMaterial->Get(AI_MATKEY_NAME, materialName) == AI_SUCCESS) {
            std::string nameStr = materialName.C_Str();
            std::print("Processing material: {}\n", materialName.C_Str());
        }

        Material material;
        nodeMaterial->Get(AI_MATKEY_BASE_COLOR, material.baseColour);
        nodeMaterial->Get(AI_MATKEY_METALLIC_FACTOR, material.metallic);
        nodeMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, material.roughness);

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

        if (const auto albedoTexture = pullTextureFromMaterial(nodeMaterial, scene, aiTextureType_DIFFUSE)) {
            material.albedoMap = renderer.uploadTexture(*albedoTexture);
            material.mapFlags |= static_cast<std::uint32_t>(MapFlags::HasAlbedoMap);
        }

        if (const auto normalTexture = pullTextureFromMaterial(nodeMaterial, scene, aiTextureType_NORMALS)) {
            material.normalMap = renderer.uploadTexture(*normalTexture);
            material.mapFlags |= static_cast<std::uint32_t>(MapFlags::HasNormalMap);
        }

        if (const auto metallicTexture = pullTextureFromMaterial(nodeMaterial, scene, aiTextureType_METALNESS)) {
            material.metallicMap = renderer.uploadTexture(*metallicTexture);
            material.mapFlags |= static_cast<std::uint32_t>(MapFlags::HasMetallicMap);
        }

        if (const auto roughnessTexture = pullTextureFromMaterial(nodeMaterial, scene, aiTextureType_DIFFUSE_ROUGHNESS)) {
            material.roughnessMap = renderer.uploadTexture(*roughnessTexture);
            material.mapFlags |= static_cast<std::uint32_t>(MapFlags::HasRoughnessMap);
        }

        // todo combined roughness/metallic
        return material;
    }

    Mesh Engine::processNodeMesh(const aiMesh* mesh, const aiScene* scene) {
        std::vector<Vertex> vertices;
        vertices.reserve(mesh->mNumVertices);
        std::vector<std::uint32_t> indices;
        indices.reserve(mesh->mNumFaces * 3); // todo make it known this is only guaranteed because of aiProcess_Triangulate

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

        const aiMaterial* nodeMaterial = scene->mMaterials[mesh->mMaterialIndex];
        auto meshMaterial = std::make_shared<Material>(processNodeMaterial(nodeMaterial, scene));

        auto newMesh = Mesh(std::move(vertices), std::move(indices), std::move(meshMaterial));
        return newMesh;
    }

    SceneObject* Engine::buildNodeSceneObject(const aiNode* node, const aiScene* scene) {
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

            auto objectMesh = std::make_shared<Mesh>(processNodeMesh(nodeMesh, scene));
            // todo renderer should handle this as required, engine shouldn't know about GPU handles
            objectMesh->setGPUHandle(renderer.uploadMesh(*objectMesh));

            sceneObject->addMesh(std::move(objectMesh));
        }

        for (std::size_t i = 0; i < node->mNumChildren; i++) {
            SceneObject* childObject = buildNodeSceneObject(node->mChildren[i], scene);
            childObject->getTransform().setParent(sceneObject->getTransform());
        }

        return sceneObject;
    }

    SceneObject* Engine::buildAssetSceneObject(const std::filesystem::path& path) {
        static Assimp::Importer importer;

        const aiScene* scene = importer.ReadFile(
            path.string(),
            aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices |
            aiProcess_FlipUVs |
            aiProcess_GenSmoothNormals
        );

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            std::println(stderr, "Error while building asset SceneObject ({}): {}", path.filename().string(), importer.GetErrorString());
            return nullptr;
        }

        return buildNodeSceneObject(scene->mRootNode, scene);
    }
}
