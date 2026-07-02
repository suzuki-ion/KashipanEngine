#pragma once

#include <KashipanEngine.h>

namespace KashipanEngine {

class GameScene final : public Scene {
public:
    GameScene();
    ~GameScene() override;

    void Initialize() override;

protected:
    void OnUpdate() override;

private:
    SceneDefaultVariables *sceneDefaultVariables_ = nullptr;
};

} // namespace KashipanEngine