#include "Engine.h"
#include "Scene.h"
#include "utils/Paths.h"

int main(const int argc, char** argv) {
    Fate::Engine engine;

    auto mainScene = std::make_unique<Fate::Scene>("Main");
    engine.setActiveScene(std::move(mainScene));

    const auto carModelPath = Fate::PathUtils::getEnginePath() / "resources/Models/mercevo2/1990 Mercedes-Benz 190 Evo II.glb";
    const auto carAsset = engine.buildAssetSceneObject(carModelPath);
    carAsset->setName("Mercedes-Benz 190 Evo II");
    carAsset->getTransform().setPosition({-4.0f, -0.5f, 0.0f});
    engine.getActiveScene()->addObject(*carAsset);

    const auto helmetModelPath = Fate::PathUtils::getEnginePath() / "resources/Models/damagedhelmet/damagedhelmet.glb";
    const auto helmetAsset = engine.buildAssetSceneObject(helmetModelPath);
    helmetAsset->setName("Damaged Helmet");
    engine.getActiveScene()->addObject(*helmetAsset);

    engine.run();
    return 0;
}
