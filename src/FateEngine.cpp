#include "FateEngine.h"

#include <print>

#include "Scene.h"
#include "assimp/Importer.hpp"
#include "assimp/mesh.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "util/Utils.h"

void FateEngine::run() {
    while (!glfwWindowShouldClose(renderer.getWindow())) {
        glfwPollEvents();

        const double currentTime = glfwGetTime();
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

void processMaterial(aiMaterial* material, const aiScene* scene) {
    // aiString texturePath;
    //
    // // diffuse/albedo
    // if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS) {
    //     // embedded texture
    //     if (auto texture = scene->GetEmbeddedTexture(texturePath.C_Str())) {
    //         // compressed texture
    //         if (texture->mHeight == 0) {
    //             auto compressedData = reinterpret_cast<unsigned char*>(texture->pcData);
    //             auto dataSize = texture->mWidth;
    //         }
    //     }
    // }
}

Mesh processMesh(const aiMesh* mesh, const aiScene* scene) {
    std::vector<Vertex> vertices;
    vertices.reserve(mesh->mNumVertices);
    std::vector<std::uint32_t> indices;
    indices.reserve(mesh->mNumFaces * 3); // todo make it known this is only guaranteed because of aiProcess_Triangulate

    for (std::size_t i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;

        vertex.position = {mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z};

        if (mesh->HasNormals()) {
            vertex.normal = {mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z};
        }

        if (mesh->HasTextureCoords(0)) {
            vertex.uv = {mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y};
        }
        else {
            vertex.uv = {0.0f, 0.0f};
        }

        vertices.push_back(vertex);
    }

    for (std::size_t i = 0; i < mesh->mNumFaces; i++) {
        const aiFace face = mesh->mFaces[i];
        for (std::size_t j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }

    // aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
    // processMaterial(material, scene);

    return Mesh(std::move(vertices), std::move(indices));
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

        auto objectMesh = std::make_shared<Mesh>(processMesh(nodeMesh, scene));
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
