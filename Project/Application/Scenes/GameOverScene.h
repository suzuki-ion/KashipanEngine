#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class GameOverScene final : public SceneBase {
public:
    GameOverScene();
    ~GameOverScene() override;

    void Initialize() override;

protected:
    void OnUpdate() override;

private:
<<<<<<< HEAD:Project/Application/Scenes/GameOverScene.h
    Sphere *player_ = nullptr;

    // プレイヤー移動範囲
    Vector3 playerMoveMin_ = Vector3{ -5.0f, 0.0f, -2.0f };
    Vector3 playerMoveMax_ = Vector3{ 5.0f, 0.0f, 3.0f };

    AudioManager::PlayHandle bgmPlay_ = AudioManager::kInvalidPlayHandle;
=======
    SceneDefaultVariables *sceneDefaultVariables_ = nullptr;
>>>>>>> TD2_3:Application/Scenes/GameOverScene.h
};

} // namespace KashipanEngine
