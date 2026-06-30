#include "FateEngine.h"

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

void FateEngine::run() {
    bool running = true;
    SDL_Event event; // todo untangle render/engine wrt window

    while (running) {
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);

            if (event.type == SDL_EVENT_QUIT) {
                running = false;
                break;
            }
        }

        const double currentTime = static_cast<double>(SDL_GetPerformanceCounter()) / static_cast<double>(SDL_GetPerformanceFrequency());
        deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        renderer.render(*activeScene);
        renderer.drawEditorUI(*activeScene, deltaTime);
        renderer.endRender();
    }
}

void FateEngine::setActiveScene(std::unique_ptr<Scene> scene) {
    activeScene = std::move(scene);
}

Material FateEngine::processNodeMaterial(const aiMaterial* nodeMaterial, const aiScene* scene) {
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

    constexpr aiTextureType textureType = aiTextureType_DIFFUSE; // todo parameter

    aiString texturePath;
    if (nodeMaterial->GetTexture(textureType, 0, &texturePath) == AI_SUCCESS) {
        std::print("- Found {} texture at {}", aiTextureTypeToString(textureType), texturePath.C_Str());
        material.mapFlags |= static_cast<std::uint32_t>(MapFlags::HasAlbedoMap);

        if (const aiTexture* texture = scene->GetEmbeddedTexture(texturePath.C_Str())) {
            // is embedded texture
            std::print(", embedded");

            if (texture->mHeight == 0) {
                // is compressed texture
                std::print(", compressed ({})", texture->achFormatHint);

                const auto pngBuffer = reinterpret_cast<const uint8_t *>(texture->pcData);
                const std::size_t pngBufferSize = texture->mWidth;

                std::print(", size {}", FileUtils::prettyBytes(pngBufferSize));

                std::uint32_t width, height;
                const auto decodedData = FileUtils::decodePng(pngBuffer, pngBufferSize, width, height);

                std::print(", dimensions {}x{}\n", width, height);

                // material.albedoMapHandle = renderer.uploadTexture({width, height, decodedData.get()});
            }
            else {
                // is uncompressed texture
                const auto width = texture->mWidth;
                const auto height = texture->mHeight;
                std::print(", uncompressed, dimensions ({}x{})\n", width, height);

                // todo guesswork, test
                const auto pixelData = reinterpret_cast<uint8_t *>(texture->pcData);
                // material.albedoMapHandle = renderer.uploadTexture({width, height, pixelData});
            }
        }
    }

    return material;
}

Mesh FateEngine::processNodeMesh(const aiMesh* mesh, const aiScene* scene) {
    std::vector<Vertex> vertices;
    vertices.reserve(mesh->mNumVertices);
    std::vector<std::uint32_t> indices;
    indices.reserve(mesh->mNumFaces * 3); // todo make it known this is only guaranteed because of aiProcess_Triangulate

    for (std::size_t i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;

        // if (mesh->HasVertexColors(0)) {
        //     vertex.baseColour = {mesh->mColors[0][i].r, mesh->mColors[0][i].g, mesh->mColors[0][i].b, mesh->mColors[0][i].a};
        // }

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

SceneObject* FateEngine::buildNodeSceneObject(const aiNode* node, const aiScene* scene) {
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
        // objectMesh->setGPUHandle(renderer.uploadMesh(*objectMesh));

        sceneObject->addMesh(std::move(objectMesh));
    }

    for (std::size_t i = 0; i < node->mNumChildren; i++) {
        SceneObject* childObject = buildNodeSceneObject(node->mChildren[i], scene);
        childObject->getTransform().setParent(sceneObject->getTransform());
    }

    return sceneObject;
}

SceneObject* FateEngine::buildAssetSceneObject(const std::filesystem::path& path) {
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
