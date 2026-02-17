#include "PlayerDieParticleManager.h"
#include "PlayerDieParticle.h"
#include "Objects/Components/Health.h"

namespace KashipanEngine {

PlayerDieParticleManager::PlayerDieParticleManager()
    : ISceneComponent("PlayerDieParticleManager", 1)
{
}

void PlayerDieParticleManager::Update() {
    if (!player_) return;

    auto* health = player_->GetComponent3D<Health>();
    if (!health) return;

    bool currentlyAlive = health->IsAlive();

    // 生きている状態から死んだ状態への変化を検出
    if (wasAlive_ && !currentlyAlive) {
        auto* transform = player_->GetComponent3D<Transform3D>();
        if (transform) {
            SpawnParticles(transform->GetTranslate());
        }
    }

    // 現在の状態を保存
    wasAlive_ = currentlyAlive;
}

void PlayerDieParticleManager::InitializeParticlePool(int count) {
    particleCount_ = count;

    auto* ctx = GetOwnerContext();
    if (!ctx) return;

    for (int i = 0; i < particleCount_; ++i) {
        auto obj = std::make_unique<Box>();
        obj->SetName("PlayerDieParticle_" + std::to_string(i));

        if (auto* tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetScale(Vector3{ 0.0f, 0.0f, 0.0f });
        }

        if (auto* mt = obj->GetComponent3D<Material3D>()) {
            mt->SetColor(Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
        }

        obj->RegisterComponent<PlayerDieParticle>(particleConfig_);

        if (screenBuffer_) {
            obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
        }
        if (shadowMapBuffer_) {
            //obj->AttachToRenderer(shadowMapBuffer_, "Object3D.ShadowMap.DepthOnly");
        }

        auto* raw = obj.get();
        particlePool_.push_back(raw);
        ctx->AddObject3D(std::move(obj));
    }
}

void PlayerDieParticleManager::SpawnParticles(const Vector3& position) {
    for (auto* particle : particlePool_) {
        if (!particle) continue;

        auto* particleComp = particle->GetComponent3D<PlayerDieParticle>();
        if (!particleComp) continue;

        // パーティクルを発生させる前に最新の設定を適用
        particleComp->SetConfig(particleConfig_);
        particleComp->Spawn(position);
    }
}

} // namespace KashipanEngine
