#include "WallBreakParticle.h"

namespace KashipanEngine {

    WallBreakParticle::WallBreakParticle(const ParticleConfig& config)
        : IObjectComponent3D("BombExplosionParticle", 1),
        config_(config),
        isAlive_(false)
    {}

    std::optional<bool> WallBreakParticle::Initialize() {
        transform_ = GetOwner3DContext()->GetComponent<Transform3D>();
        if (!transform_) {
            return false;
        }

        transform_->SetScale(Vector3{ 0.0f, 0.0f, 0.0f });
        isActive_ = false;
        elapsed_ = 0.0f;

        return true;
    }

    std::optional<bool> WallBreakParticle::Update() {
        if (!isActive_ || !transform_) {
            return true;
        }

        const float dt = std::max(0.0f, GetDeltaTime());
        elapsed_ += dt;

        if (elapsed_ >= config_.lifeTimeSec) {
            isActive_ = false;
            transform_->SetScale(Vector3{ 0.0f, 0.0f, 0.0f });
            isAlive_ = false;
            return true;
        }

        velocity_.y -= config_.gravity * dt;
        velocity_ *= config_.damping;

        Vector3 pos = transform_->GetTranslate();
        pos += velocity_ * dt;
        transform_->SetTranslate(pos);

        const float t = elapsed_ / config_.lifeTimeSec;
        const float scaleMultiplier = EaseOutCubic(1.0f, 0.0f, t);
        transform_->SetScale(config_.baseScale * scaleMultiplier);

        return true;
    }

    void WallBreakParticle::Spawn(const Vector3& position) {
        if (!transform_) return;

        const float theta = GetRandomFloat(0.0f, 6.28f);
        const float phi = GetRandomFloat(0.0f, 6.28f);
        const float speed = config_.initialSpeed + GetRandomFloat(-config_.speedVariation, config_.speedVariation);

        velocity_ = Vector3{
            std::sin(phi) * std::cos(theta),
            std::cos(phi),
            std::sin(phi) * std::sin(theta)
        } *speed;

        transform_->SetTranslate(position);
        transform_->SetScale(config_.baseScale);

        isActive_ = true;
        isAlive_ = true;
        elapsed_ = 0.0f;
    }

    bool WallBreakParticle::IsAlive() const {
        return isAlive_;
    }

#if defined(USE_IMGUI)
    void WallBreakParticle::ShowImGui() {
        ImGui::DragFloat("Initial Speed", &config_.initialSpeed, 0.1f, 0.0f, 20.0f);
        ImGui::DragFloat("Speed Variation", &config_.speedVariation, 0.1f, 0.0f, 10.0f);
        ImGui::DragFloat("Life Time (sec)", &config_.lifeTimeSec, 0.05f, 0.1f, 5.0f);
        ImGui::DragFloat("Gravity", &config_.gravity, 0.1f, 0.0f, 30.0f);
        ImGui::DragFloat("Damping", &config_.damping, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat3("Base Scale", &config_.baseScale.x, 0.01f, 0.01f, 2.0f);
    }
#endif

} // namespace KashipanEngine
