#pragma once
#include <KashipanEngine.h>
#include "Utilities/Easing.h"

namespace KashipanEngine {

/// @brief BPMに合わせてオブジェクトを拡縮させるコンポーネント
class BPMScaling final : public IObjectComponent3D {
public:
    /// @brief コンストラクタ
    /// @param minScale 最小スケール（BPM進行度0.0のとき）
    /// @param maxScale 最大スケール（BPM進行度1.0のとき）
    explicit BPMScaling(Vector3 minScale = { 0.9f,0.9f,0.9f }, Vector3 maxScale = { 1.0f,1.0f,1.0f }, EaseType easeType = EaseType::Linear)
        : IObjectComponent3D("BPMScaling", 1)
        , minScale_(minScale)
        , maxScale_(maxScale)
        , easeType_(easeType) {}

    /// @brief コンストラクタ（全軸同一スケール版）
    /// @param minScaleAll 全軸の最小スケール
    /// @param maxScaleAll 全軸の最大スケール
    explicit BPMScaling(float minScaleAll, float maxScaleAll)
        : IObjectComponent3D("BPMScaling", 1)
        , minScale_(minScaleAll, minScaleAll, minScaleAll)
        , maxScale_(maxScaleAll, maxScaleAll, maxScaleAll) {}

    ~BPMScaling() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto clone = std::make_unique<BPMScaling>(minScale_, maxScale_);
        return clone;
    }

    std::optional<bool> Update() override;

    /// @brief BPM進行度の設定（外部から更新される）
    /// @param bpmProgress BPM進行度（0.0～1.0）
    void SetBPMProgress(float bpmProgress) { bpmProgress_ = bpmProgress; }

	void SetEaseType(EaseType easeType) { easeType_ = easeType; }

    /// @brief 最小スケール係数の設定
    void SetMinMaxScale(Vector3 minScale, Vector3 maxScale) { minScale_ = minScale; maxScale_ = maxScale; }

#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif

private:
    Vector3 minScale_ = { 0.9f,0.9f,0.9f };               // 最小スケール係数
    Vector3 maxScale_ = { 1.1f,1.1f,1.1f };               // 最大スケール係数
    float bpmProgress_ = 0.0f;            // BPM進行度（0.0～1.0）
	EaseType easeType_ = EaseType::Linear; // イージングタイプ
};

} // namespace KashipanEngine