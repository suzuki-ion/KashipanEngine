#include "PlayerDieParticle.h"

namespace KashipanEngine {

PlayerDieParticle::PlayerDieParticle(const ParticleConfig& config)
    : IObjectComponent3D("PlayerDieParticle", 1),
      config_(config),
      isAlive_(false)
{
}

std::optional<bool> PlayerDieParticle::Initialize() {
    transform_ = GetOwner3DContext()->GetComponent<Transform3D>();
    if (!transform_) {
        return false;
    }

    rotateVelocity_ = Vector3{
        GetRandomFloat(-1.0f, 1.0f),
        GetRandomFloat(-1.0f, 1.0f),
        GetRandomFloat(-1.0f, 1.0f)
	};

    transform_->SetScale(Vector3{ 0.0f, 0.0f, 0.0f });
    isActive_ = false;
    elapsed_ = 0.0f;

    return true;
}

std::optional<bool> PlayerDieParticle::Update() {
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

	rotation_ += rotateVelocity_ * dt;
	transform_->SetRotate(rotation_);

    const float t = elapsed_ / config_.lifeTimeSec;
    const float scaleMultiplier = EaseOutCubic(1.0f, 0.0f, t);
    transform_->SetScale(config_.baseScale * scaleMultiplier);

    return true;
}

void PlayerDieParticle::Spawn(const Vector3& position) {
    if (!transform_) return;

    // 上方向を基準にspreadAngleの範囲でランダムに広がる
    const float angleRad = GetRandomFloat(-config_.spreadAngle, config_.spreadAngle) * 0.0174533f; // 度からラジアンへ
    const float speed = config_.initialSpeed + GetRandomFloat(-config_.speedVariation, config_.speedVariation);

    // X-Z平面でのランダムな方向
    const float azimuth = GetRandomFloat(0.0f, 6.28318f); // 0〜2π

    // 上方向（Y軸正）を中心に広がる速度ベクトル
    velocity_ = Vector3{
        std::sin(angleRad) * std::cos(azimuth),
        std::cos(angleRad),
        std::sin(angleRad) * std::sin(azimuth)
    } * speed;

    transform_->SetTranslate(position);
    transform_->SetScale(config_.baseScale);

    isActive_ = true;
    isAlive_ = true;
    elapsed_ = 0.0f;
}

bool PlayerDieParticle::IsAlive() const {
    return isAlive_;
}

#if defined(USE_IMGUI)
void PlayerDieParticle::ShowImGui() {
    ImGui::DragInt("Particle Count", &particleCount_, 1, 1, 100);
    ImGui::DragFloat("Initial Speed", &config_.initialSpeed, 0.1f, 0.0f, 20.0f);
    ImGui::DragFloat("Speed Variation", &config_.speedVariation, 0.1f, 0.0f, 10.0f);
    ImGui::DragFloat("Life Time (sec)", &config_.lifeTimeSec, 0.05f, 0.1f, 5.0f);
    ImGui::DragFloat("Gravity", &config_.gravity, 0.1f, 0.0f, 30.0f);
    ImGui::DragFloat("Damping", &config_.damping, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Spread Angle (deg)", &config_.spreadAngle, 1.0f, 0.0f, 90.0f);
    ImGui::DragFloat3("Base Scale", &config_.baseScale.x, 0.01f, 0.01f, 2.0f);
}
#endif

} // namespace KashipanEngine
