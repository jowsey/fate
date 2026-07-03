#include <print>

#include "spdlog/spdlog.h"

#include "Engine.h"
#include "Scene.h"
#include "utils/Paths.h"

#ifdef _WIN32
#include <Windows.h>
#endif

int main(const int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

#ifndef NDEBUG // double negative </3
    spdlog::set_level(spdlog::level::debug);
#endif

    Fate::Engine engine;

    auto mainScene = std::make_unique<Fate::Scene>("Main");
    engine.setActiveScene(std::move(mainScene));

    const auto skybox = engine.buildCubemap({
        Fate::PathUtils::getEnginePath() / "resources/Textures/Skyboxes/canary_wharf_8k/plusX.jpeg",
        Fate::PathUtils::getEnginePath() / "resources/Textures/Skyboxes/canary_wharf_8k/minusX.jpeg",
        Fate::PathUtils::getEnginePath() / "resources/Textures/Skyboxes/canary_wharf_8k/plusY.jpeg",
        Fate::PathUtils::getEnginePath() / "resources/Textures/Skyboxes/canary_wharf_8k/minusY.jpeg",
        Fate::PathUtils::getEnginePath() / "resources/Textures/Skyboxes/canary_wharf_8k/plusZ.jpeg",
        Fate::PathUtils::getEnginePath() / "resources/Textures/Skyboxes/canary_wharf_8k/minusZ.jpeg",
    });
    engine.getActiveScene()->setSkybox(skybox);

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
