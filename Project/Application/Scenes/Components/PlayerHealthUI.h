#pragma once
#include <KashipanEngine.h>
#include <vector>

namespace KashipanEngine {

class Health;
class ScreenBuffer;
class Sprite;

class PlayerHealthUI final : public ISceneComponent {
public:
    /// @brief プレイヤーのHPを表示するUIコンポーネントを作成する
    /// @param screenBuffer 描画先のスクリーンバッファ
    PlayerHealthUI(ScreenBuffer *screenBuffer)
        : ISceneComponent("PlayerHealthUI", 1), screenBuffer_(screenBuffer) {}

    /// @brief デストラクタ
    ~PlayerHealthUI() override = default;

    /// @brief Health コンポーネントをバインドする
    /// @param health バインドする Health オブジェクト
    void SetHealth(Health *health);

    /// @brief 毎フレーム更新処理
    void Update() override;

private:
    void EnsureSprites();
    void UpdateSpriteColors();

    ScreenBuffer *screenBuffer_ = nullptr;
    Health *health_ = nullptr;

    int maxHpAtBind_ = 0;
    std::vector<Sprite *> sprites_;
};

} // namespace KashipanEngine
