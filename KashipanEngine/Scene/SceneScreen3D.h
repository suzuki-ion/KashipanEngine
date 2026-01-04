#pragma once

#include "Scene/Scene.h"

namespace KashipanEngine {

class SceneManager;

class SceneScreen3D : public Scene {
public:
    SceneScreen3D() = delete;
    ~SceneScreen3D() override = default;

protected:
    explicit SceneScreen3D(const std::string &sceneName)
        : Scene(sceneName) {}
};

} // namespace KashipanEngine
