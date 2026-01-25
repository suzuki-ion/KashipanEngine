#include "EnemySpawnParticle.h"
#include <random>

namespace KashipanEngine {

    EnemySpawnParticle::EnemySpawnParticle(const ParticleConfig& config)
        : IObjectComponent3D("EnemySpawnParticle", 20)
        , config_(config) {
    }

    std::optional<bool> EnemySpawnParticle::Initialize() {
        auto* context = GetOwnerContext();
        if (!context) return false;

        auto* owner = static_cast<Object3DContext*>(context)->GetOwner();
        if (!owner) return false;

        transform_ = transform_ = GetOwner3DContext()->GetComponent<Transform3D>();
        if (!transform_) return false;

        return true;
    }

    std::optional<bool> EnemySpawnParticle::Update() {
        if (!isActive_) return std::nullopt;

		float deltaTime = GetDeltaTime();
        elapsed_ += deltaTime;

        // 生存時間を超えたら非アクティブ化
        if (elapsed_ >= config_.lifeTimeSec) {
            isActive_ = false;
            isAlive_ = false;
            return std::nullopt;
        }

        // 上昇速度を適用（負の重力で上昇）
        velocity_.y += config_.gravity * deltaTime;
        velocity_ *= config_.damping;

        // 位置を更新
        Vector3 currentPos = transform_->GetTranslate();
        currentPos += velocity_ * deltaTime;
        transform_->SetTranslate(currentPos);

        // フェードアウト（時間経過で縮小）
        float lifeRatio = elapsed_ / config_.lifeTimeSec;
        float scale = (1.0f - lifeRatio);
        transform_->SetScale(config_.baseScale * scale);

        return true;
    }

    void EnemySpawnParticle::Spawn(const Vector3& position) {
        if (!transform_) return;

        // パーティクルを初期化
        transform_->SetTranslate(position);
        transform_->SetScale(config_.baseScale);
        elapsed_ = 0.0f;
        isActive_ = true;
        isAlive_ = true;

        // ランダムな上方向の速度を設定
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> speedDist(
            config_.initialSpeed - config_.speedVariation,
            config_.initialSpeed + config_.speedVariation
        );
        std::uniform_real_distribution<float> angleDist(0.0f, 360.0f);

        float speed = speedDist(gen);
        float angleY = angleDist(gen) * 3.14159265f / 180.0f;
        
        // 主に上方向だが、わずかに水平方向にも広がる
        velocity_.x = std::sin(angleY) * speed * 0.5f;
        velocity_.y = speed;  // 上方向
        velocity_.z = std::cos(angleY) * speed * 0.5f;
    }

    bool EnemySpawnParticle::IsAlive() const {
        return isAlive_;
    }

#if defined(USE_IMGUI)
    void EnemySpawnParticle::ShowImGui() {
        if (ImGui::TreeNode("EnemySpawnParticle")) {
            ImGui::Text("Active: %s", isActive_ ? "true" : "false");
            ImGui::Text("Alive: %s", isAlive_ ? "true" : "false");
            ImGui::Text("Elapsed: %.2f / %.2f", elapsed_, config_.lifeTimeSec);
            
            if (ImGui::TreeNode("Config")) {
                ImGui::DragInt("ParticleCount", &config_.particleCount, 1, 1, 100);
                ImGui::DragFloat("InitialSpeed", &config_.initialSpeed, 0.1f, 0.0f, 20.0f);
                ImGui::DragFloat("SpeedVariation", &config_.speedVariation, 0.1f, 0.0f, 10.0f);
                ImGui::DragFloat("LifeTimeSec", &config_.lifeTimeSec, 0.01f, 0.1f, 5.0f);
                ImGui::DragFloat("Gravity", &config_.gravity, 0.1f, -20.0f, 0.0f);
                ImGui::DragFloat("Damping", &config_.damping, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat3("BaseScale", &config_.baseScale.x, 0.1f, 0.0f, 10.0f);
                ImGui::TreePop();
            }
            
            ImGui::TreePop();
        }
    }
#endif

} // namespace KashipanEngine