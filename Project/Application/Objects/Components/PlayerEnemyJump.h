#pragma once
#include <KashipanEngine.h>
#include "PlayerCollision.h"
#include "PlayerMovement.h"

namespace KashipanEngine {

class PlayerEnemyJump final : public IObjectComponent3D {
public:
    PlayerEnemyJump() : IObjectComponent3D("PlayerEnemyJump", 1) {}
    ~PlayerEnemyJump() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        return std::make_unique<PlayerEnemyJump>();
    }

    std::optional<bool> Update() override {
        playerCollision_ = GetOwner3DContext()->GetComponent<PlayerCollision>();
        if (!playerCollision_) return false;
        playerMovement_ = GetOwner3DContext()->GetComponent<PlayerMovement>();
        if (!playerMovement_) return false;
        const bool isCollidingWithEnemy = playerCollision_->IsCollidingWithEnemy();
        const Vector3 &hitNormal = playerCollision_->GetHitNormal();
        if (isCollidingWithEnemy && hitNormal.y < 0.5f) {
            playerMovement_->Jump(true);
        }
        return true;
    }

#ifdef USE_IMGUI
    void ShowImGui() override {
        ImGui::Text("PlayerEnemyJump Component");
    }
#endif

private:
    PlayerCollision *playerCollision_ = nullptr;
    PlayerMovement *playerMovement_ = nullptr;
};

} // namespace KashipanEngine