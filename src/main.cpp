#include <ostream>

#include "Engine.h"
#include "Scene.h"
#include "utils/Paths.h"

int main(const int argc, char** argv) {
    Fate::Engine engine;

    auto mainScene = std::make_unique<Fate::Scene>("Main");
    engine.setActiveScene(std::move(mainScene));

    const auto carModelPath = Fate::PathUtils::getEnginePath() / "resources/Models/mercevo2/1990 Mercedes-Benz 190 Evo II.glb";
    const auto carAsset = engine.buildAssetSceneObject(carModelPath);
    engine.getActiveScene()->addObject(*carAsset);

    engine.run();
    return 0;
}
