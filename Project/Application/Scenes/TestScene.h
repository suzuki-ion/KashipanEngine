#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class TestScene final : public Scene {
public:
    TestScene();
    ~TestScene() override;

    void Initialize() override;

protected:
    void OnUpdate() override;

private:
    SceneDefaultVariables *sceneDefaultVariables_ = nullptr;
};

} // namespace KashipanEngine
