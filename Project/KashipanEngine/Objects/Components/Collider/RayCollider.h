#pragma once
#include <reactphysics3d/reactphysics3d.h>

#include "Objects/Components/Collider/ICollider.h"
#include "Scene/Components/SceneObjectCollider.h"
#include "Scene/SceneContext.h"
#include "Math/Vector3.h"

namespace KashipanEngine {

/// @brief 3D用のレイキャスト専用コライダー
/// @details 常駐する当たり判定形状は持たない（BuildColliderInfo3Dはnulloptを返す）ため、
///          OnCollisionEnter等のコールバックは呼ばれない。代わりにCastRay()を呼び出した
///          タイミングでシーンの物理ワールドへレイキャストを行い、結果を直接受け取る。
class RayCollider final : public ICollider {
public:
    RayCollider() : ICollider("RayCollider", Shape::Ray, false, GetComponentTypeID<RayCollider>()) {}
    ~RayCollider() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<RayCollider>();
        ptr->direction_ = direction_;
        ptr->maxDistance_ = maxDistance_;
        ptr->SetTrigger(IsTrigger());
        ptr->CopySyncSettingsFrom(*this);
        return ptr;
    }

    void SetDirection(const Vector3 &direction) { direction_ = direction; }
    void SetMaxDistance(float maxDistance) { maxDistance_ = maxDistance; }
    const Vector3 &GetDirection() const noexcept { return direction_; }
    float GetMaxDistance() const noexcept { return maxDistance_; }

    /// @brief 現在のワールド座標・方向でレイキャストを行う
    /// @param outHit ヒットした場合の情報の格納先
    /// @return ヒットした場合はtrue
    bool CastRay(HitInfo3D &outHit) const {
        auto *sceneContext = GetOwnerSceneContext();
        auto *sceneObjectCollider = sceneContext ? sceneContext->GetComponent<SceneObjectCollider>() : nullptr;
        auto *world = sceneObjectCollider ? sceneObjectCollider->GetCollider()->GetPhysicsWorld() : nullptr;
        if (!world) return false;

        const Vector3 origin = GetSyncedOwnerPosition();
        const Vector3 dir = GetSyncedOwnerRotation().RotateVector(direction_).Normalize();
        const Vector3 end = origin + dir * maxDistance_;

        const reactphysics3d::Ray ray(
            reactphysics3d::Vector3(origin.x, origin.y, origin.z),
            reactphysics3d::Vector3(end.x, end.y, end.z));

        struct Callback final : reactphysics3d::RaycastCallback {
            bool hit = false;
            HitInfo3D info{};
            reactphysics3d::decimal notifyRaycastHit(const reactphysics3d::RaycastInfo &raycastInfo) override {
                hit = true;
                info.isHit = true;
                info.normal = Vector3{raycastInfo.worldNormal.x, raycastInfo.worldNormal.y, raycastInfo.worldNormal.z};
                info.penetration = 0.0f;
                return raycastInfo.hitFraction;
            }
        } callback;

        world->raycast(ray, &callback);
        if (callback.hit) outHit = callback.info;
        return callback.hit;
    }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        ICollider::ShowImGui();
        ImGui::DragFloat3("Direction", &direction_.x, 0.01f);
        ImGui::DragFloat("MaxDistance", &maxDistance_, 0.01f, 0.0f);
    }
#endif
    JSON SaveToJson() const override {
        JSON json = ICollider::SaveToJson();
        json["direction"] = ToJSON(direction_);
        json["maxDistance"] = maxDistance_;
        return json;
    }
    bool LoadFromJson(const JSON &json) override {
        ICollider::LoadFromJson(json);
        if (json.contains("direction")) direction_ = FromJSON<Vector3>(json["direction"]);
        maxDistance_ = json.value("maxDistance", 10.0f);
        return true;
    }

private:
    Vector3 direction_{ 0.0f, -1.0f, 0.0f };
    float maxDistance_ = 10.0f;
};

REGISTER_COMPONENT_OBJECT(RayCollider)

} // namespace KashipanEngine
