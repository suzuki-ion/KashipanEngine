#pragma once

#include "Scene/Scene.h"

namespace KashipanEngine {

class SceneManager;

class SceneScreen2D : public Scene {
public:
    SceneScreen2D() = delete;
    ~SceneScreen2D() override = default;

protected:
    explicit SceneScreen2D(const std::string &sceneName)
        : Scene(sceneName) {}
};

} // namespace KashipanEngine
