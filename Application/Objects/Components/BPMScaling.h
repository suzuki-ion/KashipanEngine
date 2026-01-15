#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

/// @brief BPMに合わせてオブジェクトを拡縮させるコンポーネント
class BPMScaling final : public IObjectComponent3D {
public:
    /// @brief コンストラクタ
    /// @param minScale 最小スケール（BPM進行度0.0のとき）
    /// @param maxScale 最大スケール（BPM進行度1.0のとき）
    explicit BPMScaling(float minScale = 0.9f, float maxScale = 1.1f)
        : IObjectComponent3D("BPMScaling", 1)
        , minScale_(minScale)
        , maxScale_(maxScale) {}

    ~BPMScaling() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto clone = std::make_unique<BPMScaling>(minScale_, maxScale_);
        return clone;
    }

    std::optional<bool> Update() override;

    /// @brief BPM進行度の設定（外部から更新される）
    /// @param bpmProgress BPM進行度（0.0～1.0）
    void SetBPMProgress(float bpmProgress) { bpmProgress_ = bpmProgress; }

    /// @brief 最小スケール係数の設定
    void SetMinScale(float scale) { minScale_ = scale; }

    /// @brief 最大スケール係数の設定
    void SetMaxScale(float scale) { maxScale_ = scale; }

    void ShowImGui() override;

private:
    float minScale_ = 0.9f;               // 最小スケール係数
    float maxScale_ = 1.1f;               // 最大スケール係数
    float bpmProgress_ = 0.0f;            // BPM進行度（0.0～1.0）
};

} // namespace KashipanEngine