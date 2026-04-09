#pragma once
#pragma once

#include <KashipanEngine.h>

namespace KashipanEngine {

class StageGroundGenerator;
class StageNoiseWallController;
class StageGoalPlaneController;
class GameSceneUIController;
class PlayerMovementController;
class VignetteEffect;

class GameScene final : public SceneBase {
public:
    GameScene();
    ~GameScene() override;

    void Initialize() override;

protected:
    void OnUpdate() override;

private:
    enum class PlayState {
        Playing,
        GameOver,
        Cleared,
    };

    SceneDefaultVariables *sceneDefaultVariables_ = nullptr;
    Object3DBase *player_ = nullptr;
    PlayerMovementController *playerMovementController_ = nullptr;
    StageGroundGenerator *stageGroundGenerator_ = nullptr;
    StageNoiseWallController *noiseWallController_ = nullptr;
    StageGoalPlaneController *goalPlaneController_ = nullptr;
    GameSceneUIController *gameSceneUIController_ = nullptr;

    VignetteEffect *vignetteEffect_ = nullptr;
    Vector4 baseVignetteColor_{0.0f, 0.25f, 0.0f, 1.0f};

    float gameOverWallDangerDistance_ = 32.0f;
    float stageBoundaryRadius_ = 64.0f * 8.0f;

    PlayState playState_ = PlayState::Playing;
};

} // namespace KashipanEngine