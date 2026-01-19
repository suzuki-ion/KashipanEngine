#pragma once

#include "Scene/Components/ISceneComponent.h"
#include "Assets/TextureManager.h"
#include "Objects/GameObjects/2D/Sprite.h"
#include "Graphics/ScreenBuffer.h"

#include <array>

namespace KashipanEngine {

class Health;

class ResultUI final : public ISceneComponent {
public:
    /// @brief リザルト表示用 UI コンポーネントを作成する
    /// @param screenBuffer 描画先のスクリーンバッファ
    /// @param health プレイヤーの Health コンポーネント
    ResultUI(
        ScreenBuffer *screenBuffer,
        Health *health)
        : ISceneComponent("ResultUI", 1)
        , screenBuffer_(screenBuffer)
        , health_(health) {}

    /// @brief デストラクタ
    ~ResultUI() override = default;

    /// @brief 初期化処理
    void Initialize() override;
    /// @brief 毎フレーム更新処理
    void Update() override;

    /// @brief リザルトの開始アニメーションを表示する
    void ShowStart();

    /// @brief JustDodge カウントを設定する
    /// @param c カウント値
    void SetJustDodgeCount(int c) { justDodgeCount_ = (c < 0) ? 0 : c; }

    /// @brief アニメーション中かどうかを取得する
    bool IsAnimating() const { return isAnimating_; }
    /// @brief アニメーションが終了したかを取得する
    bool IsAnimationFinished() const { return isAnimationFinished_; }

private:
    void SetVisible(bool visible);

    ScreenBuffer *screenBuffer_ = nullptr;

    Health *health_ = nullptr;

    std::array<TextureManager::TextureHandle, 10> digitTextures_{};

    std::array<Sprite *, 5> scoreSprites_{};
    Sprite *scoreTextSprite_ = nullptr;
    Sprite *clearLogoSprite_ = nullptr;

    TextureManager::TextureHandle scoreTextTexture_ = TextureManager::kInvalidHandle;
    TextureManager::TextureHandle clearLogoTexture_ = TextureManager::kInvalidHandle;

    bool visible_ = false;

    float animTimeSec_ = 0.0f;
    bool isAnimating_ = false;
    bool isAnimationFinished_ = false;

    int justDodgeCount_ = 0;
};

} // namespace KashipanEngine
