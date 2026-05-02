#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class Rect;

class EngineLogoScene final : public SceneBase {
public:
    EngineLogoScene(const std::string &nextSceneName = "TitleScene");
    ~EngineLogoScene() override;

    void Initialize() override;

protected:
    void OnUpdate() override;

private:
    SceneDefaultVariables *sceneDefaultVariables_ = nullptr;

    Sprite *logoGearSprite_ = nullptr;
    Sprite *logoBreadSprite_ = nullptr;
    Sprite *logoTextSprite_ = nullptr;

    // アニメーションで使用する Rect オブジェクトを事前生成してポインタ保持
    Rect *animationRect_ = nullptr;

    // アニメーション制御
    float elapsedTime_ = 0.0f;
    float prevElapsedTime_ = 0.0f;
    float animationStartOffset_ = 1.0f; // アニメーション開始調整用
};

} // namespace KashipanEngine
