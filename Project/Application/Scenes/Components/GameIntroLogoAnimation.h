#pragma once
#include <KashipanEngine.h>
#include "Scene/Components/SceneDefaultVariables.h"

namespace KashipanEngine {

class GameIntroLogoAnimation final : public ISceneComponent {
public:
    /// @brief ロゴイントロ演出コンポーネントを作成する
    /// @param screenBuffer 表示先のスクリーンバッファ (ignored)
    explicit GameIntroLogoAnimation(ScreenBuffer* /*screenBuffer*/)
        : ISceneComponent("GameIntroLogoAnimation", 1) {}

    /// @brief デストラクタ
    ~GameIntroLogoAnimation() override = default;

    /// @brief コンポーネント初期化処理
    void Initialize() override;
    /// @brief 毎フレーム更新処理
    void Update() override;

    /// @brief ロゴ再生を開始する
    void Play();

private:
    void SetVisible(bool visible);

    SceneDefaultVariables* sceneDefault_ = nullptr;

    Sprite* logoSprite_ = nullptr;
    TextureManager::TextureHandle logoTexture_ = TextureManager::kInvalidHandle;

    bool playing_ = false;
    float elapsed_ = 0.0f;
};

} // namespace KashipanEngine
