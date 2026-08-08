#pragma once
#include <reactphysics3d/reactphysics3d.h>

#include "Objects/Components/Collider/ICollider.h"
#include "Scene/Components/SceneObjectCollider.h"
#include "Scene/SceneContext.h"
#include "Math/Vector3.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief 3D用のレイキャスト専用コライダー
/// @details 常駐する当たり判定形状は持たない（BuildColliderInfo3Dはnulloptを返す）ため、
///          他のICollider派生（Box/Sphere/Capsule等）のようなシェイプ同士の接触判定システムには乗らない。
///          代わりに、OnCollisionEnter/Stay/Exitのいずれかが設定されている間だけ毎フレームUpdate()で
///          レイキャストを行い、前フレームのヒット対象との比較からEnter/Stay/Exitを自前で判定・発火する。
///          CastRay()で任意のタイミングの単発レイキャストも行える（この自動判定とは独立して動作する）。
class RayCollider final : public ICollider {
public:
    RayCollider() : ICollider("RayCollider", Shape::Ray, false, GetComponentTypeID<RayCollider>()) {
        ADD_MEMBER_VARIABLE(direction_);
        ADD_MEMBER_VARIABLE(maxDistance_);
    }
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
    /// @param outHit ヒットした場合の情報の格納先（selfObject/otherObject/selfCollider/otherColliderも設定される）
    /// @return ヒットした場合はtrue
    bool CastRay(HitInfo3D &outHit) const {
        return CastRayInternal(outHit);
    }

protected:
    void Update() override {
        // 誰もEnter/Stay/Exitを監視していない場合は、毎フレームのレイキャストを省略する
        if (!GetOnCollisionEnter3D() && !GetOnCollisionStay3D() && !GetOnCollisionExit3D()) {
            if (wasHit_) DispatchExit();
            return;
        }

        HitInfo3D hit{};
        const bool isHitNow = CastRayInternal(hit);
        ICollider *newOtherCollider = isHitNow ? hit.otherCollider : nullptr;
        ComponentRef newOtherColliderRef = newOtherCollider ? newOtherCollider->GetComponentRef() : ComponentRef{};

        // ヒット対象が前フレームから入れ替わった場合は、まず古い対象のExitを発火してから新しい対象のEnterへ進む
        if (wasHit_ && newOtherColliderRef != lastHitColliderRef_) {
            DispatchExit();
        }

        if (isHitNow) {
            if (!wasHit_ && GetOnCollisionEnter3D()) GetOnCollisionEnter3D()(hit);
            if (GetOnCollisionStay3D()) GetOnCollisionStay3D()(hit);
            wasHit_ = true;
            lastHitColliderRef_ = newOtherColliderRef;
            lastHitObjectID_ = hit.otherObject ? hit.otherObject->GetObjectID() : UUID128();
        } else if (wasHit_) {
            DispatchExit();
        }
    }

    /// @brief 無効化・破棄時にヒット中であれば最後にExitを発火してから登録解除する
    void Finalize() override {
        if (wasHit_) DispatchExit();
        ICollider::Finalize();
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ICollider::ShowImGui();
        ImGui::DragFloat3(TranslationLabel("component.raycollider.direction"), &direction_.x, 0.01f);
        ImGui::DragFloat(TranslationLabel("component.raycollider.maxdistance"), &maxDistance_, 0.01f, 0.0f);
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
    /// @brief 現在のワールド座標・方向でレイキャストを行い、ヒットした相手（オブジェクト/コライダー）込みで結果を得る
    bool CastRayInternal(HitInfo3D &outHit) const {
        auto *sceneContext = GetOwnerSceneContext();
        auto *sceneObjectCollider = sceneContext ? sceneContext->GetComponent<SceneObjectCollider>() : nullptr;
        auto *colliderSystem = sceneObjectCollider ? sceneObjectCollider->GetCollider() : nullptr;
        auto *world = colliderSystem ? colliderSystem->GetPhysicsWorld() : nullptr;
        if (!world) return false;

        const Vector3 origin = GetSyncedOwnerPosition();
        const Vector3 dir = GetSyncedOwnerRotation().RotateVector(direction_).Normalize();
        const Vector3 end = origin + dir * maxDistance_;

        const reactphysics3d::Ray ray(
            reactphysics3d::Vector3(origin.x, origin.y, origin.z),
            reactphysics3d::Vector3(end.x, end.y, end.z));

        struct Callback final : reactphysics3d::RaycastCallback {
            const Collider *colliderSystem = nullptr;
            bool hit = false;
            HitInfo3D info{};
            reactphysics3d::decimal notifyRaycastHit(const reactphysics3d::RaycastInfo &raycastInfo) override {
                hit = true;
                info.isHit = true;
                info.normal = Vector3{raycastInfo.worldNormal.x, raycastInfo.worldNormal.y, raycastInfo.worldNormal.z};
                info.penetration = 0.0f;
                // ヒットしたRP3D Collider*から、登録元のEmptyObject/ICollider（衝突相手）を逆引きする
                if (const auto *hitInfo = colliderSystem ? colliderSystem->FindInfoByHandle3D(raycastInfo.collider) : nullptr) {
                    info.otherObject = hitInfo->ownerObject;
                    info.otherCollider = hitInfo->sourceCollider;
                }
                return raycastInfo.hitFraction;
            }
        } callback;
        callback.colliderSystem = colliderSystem;

        world->raycast(ray, &callback);
        if (callback.hit) {
            outHit = callback.info;
            outHit.selfObject = const_cast<EmptyObject *>(GetOwnerObject());
            outHit.selfCollider = const_cast<RayCollider *>(this);
        }
        return callback.hit;
    }

    /// @brief 保持中のヒット状態に基づいてOnCollisionExit3Dを発火し、状態をリセットする
    /// @details lastHitColliderRef_/lastHitObjectID_はフレームをまたいで保持されるため、生ポインタではなく
    ///          ComponentRef/UUIDで保持し、使う直前にここで解決する（プールのスロット再利用対策）
    void DispatchExit() {
        if (GetOnCollisionExit3D()) {
            auto *sceneCtx = GetOwnerSceneContext();
            HitInfo3D exitInfo{};
            exitInfo.isHit = false;
            exitInfo.selfObject = const_cast<EmptyObject *>(GetOwnerObject());
            exitInfo.otherObject = sceneCtx ? sceneCtx->GetSceneObject(lastHitObjectID_) : nullptr;
            exitInfo.selfCollider = this;
            exitInfo.otherCollider = sceneCtx ? static_cast<ICollider *>(sceneCtx->ResolveComponent(lastHitColliderRef_)) : nullptr;
            GetOnCollisionExit3D()(exitInfo);
        }
        wasHit_ = false;
        lastHitColliderRef_ = ComponentRef{};
        lastHitObjectID_ = UUID128();
    }

    Vector3 direction_{ 0.0f, -1.0f, 0.0f };
    float maxDistance_ = 10.0f;

    // --- 実行時状態（保存されない） ---
    bool wasHit_ = false;
    ComponentRef lastHitColliderRef_;
    UUID128 lastHitObjectID_;
};

REGISTER_COMPONENT_OBJECT(RayCollider)

} // namespace KashipanEngine
