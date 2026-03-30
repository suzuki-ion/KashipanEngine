#pragma once

#include <KashipanEngine.h>

namespace KashipanEngine {

class GameScene final : public SceneBase {
public:
    GameScene();
    ~GameScene() override;

    void Initialize() override;

protected:
    void OnUpdate() override;

private:
    SceneDefaultVariables *sceneDefaultVariables_ = nullptr;

    std::function<KashipanEngine::Sprite *(const std::string &, KashipanEngine::DefaultSampler)> createSpriteFunction_;
    std::function<KashipanEngine::Sprite *(const std::string &, const std::string &, KashipanEngine::DefaultSampler)> createSpriteWithTextureFunction_;
    Vector2 screenCenter_;

    bool isGameClear_ = false;
    bool isGameOver_ = false;
    float autoSceneChangeTimer_ = 0.0f;
    KashipanEngine::Sprite *backgroundSprite_ = nullptr;
    bool isNpcMode_ = false;

    AudioPlayer audioPlayer_;
};

} // namespace KashipanEngine