#pragma once
#include <KashipanEngine.h>
#include "Objects/CollisionAttributes.h"

namespace KashipanEngine {

class EnemyCollision : public IObjectComponent {
public:
    EnemyCollision()
        : IObjectComponent("EnemyCollision", 1) {}

    std::unique_ptr<IObjectComponent> Clone() const override {
        return std::make_unique<EnemyCollision>();
    }

    std::optional<bool> Initialize() override {
        ColliderInfo3D colliderInfo{};
        ColliderInfo3D::SphereShape3D enemySphere{};
        enemySphere.center = Vector3(0.0f, 0.0f, 0.0f);
        enemySphere.radius = 0.5f;
        colliderInfo.shape = enemySphere;
        colliderInfo.onCollisionEnter = [this](const HitInfo3D &hitInfo) {
            OnCollisionEnter(hitInfo);
            };
        colliderInfo.onCollisionStay = [this](const HitInfo3D &hitInfo) {
            OnCollisionStay(hitInfo);
            };
        colliderInfo.onCollisionExit = [this](const HitInfo3D &hitInfo) {
            OnCollisionExit(hitInfo);
            };
        colliderInfo.attribute = CollisionAttribute::Enemy;
        GetOwner3DContext()->RegisterComponent(std::make_unique<Collision3D>(colliderInfo));
        return true;
    }

    std::optional<bool> Update() override {
        return true;
    }

#ifdef USE_IMGUI
    void ShowImGui() override {
        ImGui::Text("EnemyCollision Component");
    }
#endif

    bool IsGrounded() const { return isGrounded_; }
    bool IsCollidingWithPlayer() const { return isCollidingWithPlayer_; }
    const Vector3 &GetHitNormal() const { return hitNormal_; }

    bool ConsumePlayerCollision() {
        if (isCollidingWithPlayer_) {
            isCollidingWithPlayer_ = false;
            return true;
        }
        return false;
    }

private:
    void OnCollisionEnter(const HitInfo3D &hitInfo) {
        if (hitInfo.otherObject->GetName() == "Ground") {
            // 法線が上向きなら地面に接触しているとみなす
            isGrounded_ = hitInfo.normal.y > 0.5f;
        } else if (hitInfo.otherObject->GetName() == "Player") {
            isCollidingWithPlayer_ = true;
            // プレイヤーにダメージを与えるなどの処理をここに追加
        }
        hitNormal_ = hitInfo.normal;
    }
    void OnCollisionStay(const HitInfo3D &hitInfo) {
        if (hitInfo.otherObject->GetName() == "Ground") {
            // 衝突判定から押し戻しベクトルを計算してエネミーを押し戻す
            auto *transform = GetOwner3DContext()->GetComponent<Transform3D>();
            if (!transform) return;

            // 衝突の法線方向にエネミーを押し戻す
            Vector3 pushBack = hitInfo.normal * hitInfo.penetration;
            pushBack.z = 0.0f;
            Vector3 newPos = transform->GetTranslate() + pushBack;
            transform->SetTranslate(newPos);
        } else if (hitInfo.otherObject->GetName() == "Player") {
            // プレイヤーにダメージを与えるなどの処理をここに追加
        }
        hitNormal_ = hitInfo.normal;
    }
    void OnCollisionExit(const HitInfo3D &hitInfo) {
        if (hitInfo.otherObject->GetName() == "Ground") {
            isGrounded_ = false;
        } else if (hitInfo.otherObject->GetName() == "Player") {
            isCollidingWithPlayer_ = false;
            // プレイヤーとの接触が終了したときの処理をここに追加
        }
        hitNormal_ = hitInfo.normal;
    }

    bool isGrounded_ = false;
    bool isCollidingWithPlayer_ = false;
    Vector3 hitNormal_ = Vector3::Zero();
};

REGISTER_COMPONENT_OBJECT(EnemyCollision)

} // namespace KashipanEngine