#include "SceneObject.h"

#include <utility>

SceneObject::SceneObject(std::string name, std::shared_ptr<Mesh> mesh, std::shared_ptr<MeshHandle> meshHandle)
    : name(std::move(name)), transform(*this), mesh(std::move(mesh)), meshHandle(std::move(meshHandle)) {
}
