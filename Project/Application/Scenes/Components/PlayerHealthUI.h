#pragma once
#include <KashipanEngine.h>
#include <vector>
#include "Scene/Components/SceneDefaultVariables.h"

namespace KashipanEngine {

class Health;
class ScreenBuffer;
class Sprite;

class PlayerHealthUI final : public ISceneComponent {
public:
    /// @brief プレイヤーのHPを表示するUIコンポーネントを作成する
    /// @param screenBuffer 描画先のスクリーンバッファ (ignored)
    PlayerHealthUI(ScreenBuffer * /*screenBuffer*/)
        : ISceneComponent("PlayerHealthUI", 1) {}

    /// @brief デストラクタ
    ~PlayerHealthUI() override = default;

    /// @brief Health コンポーネントをバインドする
    /// @param health バインドする Health オブジェクト
    void SetHealth(Health *health);

    /// @brief 毎フレーム更新処理
    void Update() override;

    void Initialize() override;

private:
    void EnsureSprites();
    void UpdateSpriteColors();

    SceneDefaultVariables *sceneDefault_ = nullptr;
    Health *health_ = nullptr;

    int maxHpAtBind_ = 0;
    std::vector<Sprite *> sprites_;
};

} // namespace KashipanEngine
