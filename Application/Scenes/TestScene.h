#pragma once

#include <memory>

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

    ScreenBuffer *offscreenBuffer1_ = nullptr;
    ScreenBuffer *offscreenBuffer2_ = nullptr;
};

} // namespace KashipanEngine
