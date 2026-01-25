#include "EnemyDieParticle.h"

namespace KashipanEngine {

    EnemyDieParticle::EnemyDieParticle(const ParticleConfig& config)
        : IObjectComponent3D("EnemyDieParticle", 1),
          config_(config),
          isAlive_(false)
    {
    }

    std::optional<bool> EnemyDieParticle::Initialize() {
        transform_ = GetOwner3DContext()->GetComponent<Transform3D>();
        if (!transform_) {
            return false;
        }

        // 初期状態は非表示
        transform_->SetScale(Vector3{ 0.0f, 0.0f, 0.0f });
        isActive_ = false;
        elapsed_ = 0.0f;

        return true;
    }

    std::optional<bool> EnemyDieParticle::Update() {
        if (!isActive_ || !transform_) {
            return true;
        }

        const float dt = std::max(0.0f, GetDeltaTime());
        elapsed_ += dt;

        // ライフタイムを超えたら非アクティブ化
        if (elapsed_ >= config_.lifeTimeSec) {
            isActive_ = false;
            transform_->SetScale(Vector3{ 0.0f, 0.0f, 0.0f });
            isAlive_ = false; // 追加: パーティクルが生きていないことを示す
            return true;
        }

        // 物理演算: 重力と減衰
        velocity_.y -= config_.gravity * dt;
        velocity_ *= config_.damping;

        // 位置更新
        Vector3 pos = transform_->GetTranslate();
        pos += velocity_ * dt;
        transform_->SetTranslate(pos);

        // スケールアニメーション (フェードアウト)
        const float t = elapsed_ / config_.lifeTimeSec;
        const float scaleMultiplier = EaseOutCubic(1.0f, 0.0f, t);
        transform_->SetScale(config_.baseScale * scaleMultiplier);

        return true;
    }

    void EnemyDieParticle::Spawn(const Vector3& position) {
        if (!transform_) return;

        // ランダムな方向に初速度を設定
        const float theta = GetRandomFloat(0.0f, 6.28f);  // 0〜2π
        const float phi = GetRandomFloat(0.0f, 6.28f);     // 0〜π
        const float speed = config_.initialSpeed + GetRandomFloat(-config_.speedVariation, config_.speedVariation);

        velocity_ = Vector3{
            std::sin(phi) * std::cos(theta),
            std::cos(phi),
            std::sin(phi) * std::sin(theta)
        } * speed;

        // 位置とスケールを設定
        transform_->SetTranslate(position);
        transform_->SetScale(config_.baseScale);

        // アクティブ化
        isActive_ = true;
        isAlive_ = true; // 追加: パーティクルが生きていることを示す
        elapsed_ = 0.0f;
    }

    bool EnemyDieParticle::IsAlive() const {
        return isAlive_;
    }

#if defined(USE_IMGUI)
    void EnemyDieParticle::ShowImGui() {
        ImGui::DragInt("Particle Count", &config_.particleCount, 1, 1, 100);
        ImGui::DragFloat("Initial Speed", &config_.initialSpeed, 0.1f, 0.0f, 20.0f);
        ImGui::DragFloat("Speed Variation", &config_.speedVariation, 0.1f, 0.0f, 10.0f);
        ImGui::DragFloat("Life Time (sec)", &config_.lifeTimeSec, 0.05f, 0.1f, 5.0f);
        ImGui::DragFloat("Gravity", &config_.gravity, 0.1f, 0.0f, 30.0f);
        ImGui::DragFloat("Damping", &config_.damping, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat3("Base Scale", &config_.baseScale.x, 0.01f, 0.01f, 2.0f);
    }
#endif

} // namespace KashipanEngine