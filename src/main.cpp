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

    engine.run();
    return 0;
}
