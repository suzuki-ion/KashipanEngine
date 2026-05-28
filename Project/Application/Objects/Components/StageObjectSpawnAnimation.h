#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class StageObjectSpawnAnimation final : public IObjectComponent3D {
public:
    StageObjectSpawnAnimation()
        : IObjectComponent3D("StageObjectSpawnAnimation", 1) {}

    std::unique_ptr<IObjectComponent> Clone() const override {
        return std::make_unique<StageObjectSpawnAnimation>();
    }

    std::optional<bool> Initialize() override {
        return ResetAnimation();
    }

    std::optional<bool> Update() override {
        if (!isAnimationStarted_) return true;

        auto *ctx = GetOwner3DContext();
        if (!ctx) return false;

        auto *tr = ctx->GetComponent<Transform3D>();
        if (!tr) return true;

        if (animationElapsed_ >= animationDelay_ + animationDuration_) {
            return true;
        }

        animationElapsed_ += GetDeltaTime() * GetGameSpeed();
        if (animationElapsed_ < animationDelay_) {
            // アニメーション開始前は何もしない
            return true;
        }
        float t = Normalize01(animationElapsed_ - animationDelay_, 0.0f, animationDuration_);

        tr->SetScale(defaultScale_);
        Vector3 position = Eased(startPosition_, endPosition_, t, animationEaseType_);
        tr->SetTranslate(position);

        return true;
    }

#ifdef USE_IMGUI
    void ShowImGui() override {}
#endif // USE_IMGUI

    bool ResetAnimation() {
        auto *ctx = GetOwner3DContext();
        if (!ctx) return false;
        auto *tr = ctx->GetComponent<Transform3D>();
        if (!tr) return false;
        defaultScale_ = tr->GetScale();
        tr->SetScale(Vector3{ 0.0f, 0.0f, 0.0f });
        animationElapsed_ = 0.0f;

        // オブジェクトのデフォルト位置をアニメーション終了位置として設定
        endPosition_ = tr->GetTranslate();
        const float z = endPosition_.z;
        endPosition_.z = 0.0f;

        Vector3 direction = endPosition_.Normalize();
        if (direction == Vector3::Zero()) {
            direction = Vector3{ 0.0f, -1.0f, 0.0f }; // デフォルトの方向（下向き）
        }
        startPosition_ = direction * kStartRadius_;

        startPosition_.z = z;
        endPosition_.z = z;

        // ディレイをランダムに設定（0～0.5秒程度）
        animationDelay_ = GetRandomFloat(0.0f, 0.5f);

        isAnimationStarted_ = false;
        return true;
    }

    void StartAnimation() {
        isAnimationStarted_ = true;
    }
    bool IsAnimationStarted() const { return isAnimationStarted_; }

private:
    Vector3 defaultScale_{ 1.0f, 1.0f, 1.0f };
    Vector3 startPosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 endPosition_{ 0.0f, 0.0f, 0.0f };
    inline static const float kStartRadius_ = 256.0f;

    float animationDuration_ = 0.5f;
    float animationElapsed_ = 0.0f;
    float animationDelay_ = 0.0f;
    EaseType animationEaseType_ = EaseType::EaseOutBack;

    bool isAnimationStarted_ = false;
};

} // namespace KashipanEngine