#include "SceneObject.h"

#include <utility>

SceneObject::SceneObject(std::string name, std::shared_ptr<MeshHandle> mesh)
    : name(std::move(name)), transform(*this), meshHandle(std::move(mesh)) {
}
