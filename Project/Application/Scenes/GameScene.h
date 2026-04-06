#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class PlayerMovementController;

class GameScene final : public SceneBase {
public:
    GameScene();
    ~GameScene() override;

    void Initialize() override;

protected:
    void OnUpdate() override;

private:
    SceneDefaultVariables *sceneDefaultVariables_ = nullptr;
    Object3DBase *player_ = nullptr;
    PlayerMovementController *playerMovementController_ = nullptr;
    SpriteProressBar *forwardSpeedBar_ = nullptr;
};

} // namespace KashipanEngine