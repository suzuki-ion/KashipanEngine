#pragma once
#include <KashipanEngine.h>

#include <vector>
#include <algorithm>

namespace KashipanEngine {

class ResultSceneAnimator final : public ISceneComponent {
public:
    ResultSceneAnimator()
        : ISceneComponent("ResultSceneAnimator", 1) {}
    ~ResultSceneAnimator() override = default;

    void Initialize() override {
        elapsed_ = 0.0f;
        playing_ = false;
        if (backgroundTransform_) {
            const auto p = backgroundTransform_->GetTranslate();
            baseX_ = p.x;
            baseZ_ = p.z;
        }
    }

    void SetTargetTransforms(std::vector<Transform2D*> transforms) {
        targetTransforms_ = std::move(transforms);
    }

    void SetBackgroundTransform(Transform2D* transform) {
        backgroundTransform_ = transform;
    }

    void PlayBackgroundDropIn() {
        if (!backgroundTransform_) return;

        const auto p = backgroundTransform_->GetTranslate();
        baseX_ = p.x;
        baseZ_ = p.z;
        elapsed_ = 0.0f;
        playing_ = true;

        backgroundTransform_->SetTranslate(Vector3(baseX_, kStartY, baseZ_));
    }

    void Update() override {
        if (!playing_ || !backgroundTransform_) return;

        elapsed_ += std::max(0.0f, GetDeltaTime());
        const float t = std::clamp(elapsed_ / durationSec_, 0.0f, 1.0f);
        const float y = EaseOutCubic(kStartY, kEndY, t);
        backgroundTransform_->SetTranslate(Vector3(baseX_, y, baseZ_));

        if (t >= 1.0f) {
            playing_ = false;
        }
    }

private:
    static constexpr float kStartY = -540.0f;
    static constexpr float kEndY = 540.0f;

    std::vector<Transform2D*> targetTransforms_;
    Transform2D* backgroundTransform_ = nullptr;

    float durationSec_ = 1.0f;
    float elapsed_ = 0.0f;
    bool playing_ = false;

    float baseX_ = 0.0f;
    float baseZ_ = 0.0f;
};

} // namespace KashipanEngine
