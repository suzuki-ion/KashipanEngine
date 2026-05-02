#pragma once
#pragma once

#include <KashipanEngine.h>

namespace KashipanEngine {

class StageGroundGenerator;
class StageNoiseWallController;
class StageGoalPlaneController;
class GameSceneUIController;
class GameOverUIController;
class GameClearUIController;
class PauseUIController;
class PlayerMovementController;
class PlayerRespawnController;
class PlayerGameOverController;
class VignetteEffect;
class ParticleManager;

class GameScene final : public SceneBase {
public:
    GameScene();
    ~GameScene() override;

    void Initialize() override;

    bool IsPlaying() const { return playState_ == PlayState::Playing; }
    bool IsGameOver() const { return playState_ == PlayState::GameOver; }
    bool IsCleared() const { return playState_ == PlayState::Cleared; }
    void SetPlayingState() { playState_ = PlayState::Playing; }
    void SetGameOverState() { playState_ = PlayState::GameOver; }
    void SetClearedState() { playState_ = PlayState::Cleared; }

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
    GameOverUIController *gameOverUIController_ = nullptr;
    GameClearUIController *gameClearUIController_ = nullptr;
    PauseUIController *pauseUIController_ = nullptr;
    ParticleManager *particleManager_ = nullptr;
    PlayerRespawnController *playerRespawnController_ = nullptr;
    PlayerGameOverController *playerGameOverController_ = nullptr;

    VignetteEffect *vignetteEffect_ = nullptr;
    Vector4 baseVignetteColor_{0.0f, 0.25f, 0.0f, 1.0f};

    float gameOverWallDangerDistance_ = 32.0f;
    float stageBoundaryRadius_ = 64.0f * 6.0f;
    bool wasPlayerGroundedPrevFrame_ = false;
    bool isPlayerRunParticleActive_ = false;
    bool groundSpawnLimitConfigured_ = false;

    bool clearSlowdownActive_ = false;
    float clearSlowdownElapsed_ = 0.0f;
    float clearSlowdownDuration_ = 1.0f;
    float clearSlowdownStartForwardSpeed_ = 0.0f;
    Vector3 clearSlowdownStartLateralVelocity_{0.0f, 0.0f, 0.0f};
    Vector3 clearSlowdownStartGravityVelocity_{0.0f, 0.0f, 0.0f};
    
    Vector3 playerSpawnPosition_{0.0f, 0.0f, -2.0f};

    PlayState playState_ = PlayState::Playing;
};

} // namespace KashipanEngine