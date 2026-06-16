#pragma once
#include <KashipanEngine.h>
#include "Objects/CollisionAttributes.h"

namespace KashipanEngine {

class PlayerCollision : public IObjectComponent3D {
public:
    PlayerCollision()
        : IObjectComponent3D("PlayerCollision", 1) {}

    std::unique_ptr<IObjectComponent> Clone() const override {
        return std::make_unique<PlayerCollision>();
    }

    std::optional<bool> Initialize() override {
        ColliderInfo3D colliderInfo{};
        ColliderInfo3D::SphereShape3D playerSphere{};
        playerSphere.center = Vector3(0.0f, 0.0f, 0.0f);
        playerSphere.radius = 0.5f;
        colliderInfo.shape = playerSphere;
        colliderInfo.onCollisionEnter = [this](const HitInfo3D &hitInfo) {
            OnCollisionEnter(hitInfo);
        };
        colliderInfo.onCollisionStay = [this](const HitInfo3D &hitInfo) {
            OnCollisionStay(hitInfo);
        };
        colliderInfo.onCollisionExit = [this](const HitInfo3D &hitInfo) {
            OnCollisionExit(hitInfo);
        };
        colliderInfo.attribute = CollisionAttribute::Player;
        colliderInfo.ignoreAttribute = CollisionAttribute::Player;
        GetOwner3DContext()->RegisterComponent(std::make_unique<Collision3D>(colliderInfo));
        if (auto *tr = GetOwner3DContext()->GetComponent<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, 0.5f, 0.0f));
            tr->SetScale(Vector3(1.0f, 1.0f, 1.0f));
        }
        return true;
    }

    std::optional<bool> Update() override {
        return true;
    }

#ifdef USE_IMGUI
    void ShowImGui() override {
        ImGui::Text("PlayerCollision Component");
    }
#endif

    bool IsGrounded() const { return isGrounded_; }
    const Vector3 &GetHitNormal() const { return hitNormal_; }

private:
    void OnCollisionEnter(const HitInfo3D &hitInfo) {
        if (hitInfo.otherObject->GetName() != "Ground") return;
        // 法線が上向きなら地面に接触しているとみなす
        isGrounded_ = hitInfo.normal.y > groundedThreshold_;
    }
    void OnCollisionStay(const HitInfo3D &hitInfo) {
        if (hitInfo.otherObject->GetName() != "Ground") return;
        // 衝突判定から押し戻しベクトルを計算してプレイヤーを押し戻す
        auto *transform = GetOwner3DContext()->GetComponent<Transform3D>();
        if (!transform) return;

        // 衝突の法線方向にプレイヤーを押し戻す
        Vector3 pushBack = hitInfo.normal * hitInfo.penetration;
        pushBack.z = 0.0f;
        Vector3 newPos = transform->GetTranslate() + pushBack;
        transform->SetTranslate(newPos);
        hitNormal_ = hitInfo.normal;
    }
    void OnCollisionExit(const HitInfo3D &hitInfo) {
        if (hitInfo.otherObject->GetName() != "Ground") return;
        isGrounded_ = false;
    }

    bool isGrounded_ = false;
    float groundedThreshold_ = 0.4f; // 法線のy成分がこの値以上なら地面とみなす
    Vector3 hitNormal_ = Vector3::Zero();
};

} // namespace KashipanEngine