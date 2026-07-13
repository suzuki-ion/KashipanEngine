#pragma once
#include <KashipanEngine.h>
#include "EnemyCollision.h"

namespace KashipanEngine {

class EnemyAliveStateController : public IObjectComponent {
public:
    EnemyAliveStateController()
        : IObjectComponent("EnemyAliveStateController", 1) {}

    std::unique_ptr<IObjectComponent> Clone() const override {
        return std::make_unique<EnemyAliveStateController>();
    }

    std::optional<bool> Update() override {
        enemyCollision_ = GetOwner3DContext()->GetComponent<EnemyCollision>();
        if (!enemyCollision_) return false;

        const bool isCollidingWithPlayer = enemyCollision_->ConsumePlayerCollision();
        const Vector3 &hitNormal = enemyCollision_->GetHitNormal();

        // プレイヤーと衝突しているかつ衝突の法線が上向きなら死亡状態にする
        if (isCollidingWithPlayer && hitNormal.y < playerCollisionThreshold_) {
            isActive_ = false;
            auto *ownerScene = GetOwnerSceneContext();
            auto *transform = GetOwner3DContext()->GetComponent<Transform3D>();
            if (auto *pm = ownerScene ? ownerScene->GetComponent<ParticleManager>() : nullptr) {
                pm->Spawn("HitEffect", transform->GetTranslate());
                pm->Spawn("HitEffect2", transform->GetTranslate());
            }
        }

        return true;
    }

#ifdef USE_IMGUI
    void ShowImGui() override {
        ImGui::Text("EnemyAliveStateController Component");
    }
#endif

    bool IsAlive() const { return isActive_; }

private:
    EnemyCollision *enemyCollision_ = nullptr;
    bool isActive_ = true;
    const float playerCollisionThreshold_ = 0.5f; // プレイヤーとの衝突とみなす法線の閾値
};

REGISTER_COMPONENT_OBJECT(EnemyAliveStateController)

} // namespace KashipanEngine