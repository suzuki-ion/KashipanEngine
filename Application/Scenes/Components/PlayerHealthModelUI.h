#pragma once
#include <KashipanEngine.h>
#include <vector>

namespace KashipanEngine {

    class Health;
    class ScreenBuffer;

    class PlayerHealthModelUI final : public ISceneComponent {
    public:
        /// @brief プレイヤーのHPを3Dモデルで表示するUIコンポーネントを作成する
        /// @param screenBuffer 描画先のスクリーンバッファ
        PlayerHealthModelUI(ScreenBuffer* screenBuffer)
            : ISceneComponent("PlayerHealthModelUI", 1), screenBuffer_(screenBuffer) {}

        /// @brief デストラクタ
        ~PlayerHealthModelUI() override = default;

        /// @brief Health コンポーネントをバインドする
        /// @param health バインドする Health オブジェクト
        void SetHealth(Health* health);

		/// @brief Transform3D コンポーネントをバインドする
        void SetTransform(Transform3D* transform);

        /// @brief 毎フレーム更新処理
        void Update() override;

    private:
        void EnsureModels();
        void UpdateModelColors();

        ScreenBuffer* screenBuffer_ = nullptr;
        Health* health_ = nullptr;
        Transform3D* parentTransform_ = nullptr;

        int maxHpAtBind_ = 0;
        std::vector<Object3DBase*> hpObject_;
        std::vector<Object3DBase*> hpOutLineObject_;
    };

} // namespace KashipanEngine
