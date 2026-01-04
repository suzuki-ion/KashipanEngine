#pragma once

#include <memory>
#include <vector>

#include "Scene/Scene.h"
#include "Graphics/ScreenBuffer.h"

namespace KashipanEngine {

class TestScene final : public Scene {
public:
    TestScene();
    ~TestScene() override;

protected:
    void OnUpdate() override;

private:
    void InitializeTestObjects();

    bool initialized_ = false;

    std::vector<std::unique_ptr<Object3DBase>> objects3D_;
    std::vector<std::unique_ptr<Object2DBase>> objects2D_;

    ScreenBuffer *offscreenBuffer_ = nullptr;
};

} // namespace KashipanEngine
