#include "SceneObjectCollider.h"

#include <algorithm>

#include "Objects/Components/Collider/CharacterController2D.h"
#include "Objects/Components/Collider/ICollider.h"

namespace KashipanEngine {

void SceneObjectCollider::RegisterCollider(ICollider *collider) {
    if (!collider) return;
    if (std::find(registeredColliders_.begin(), registeredColliders_.end(), collider) != registeredColliders_.end()) return;
    registeredColliders_.push_back(collider);
}

void SceneObjectCollider::UnregisterCollider(const ICollider *collider) {
    auto it = std::find(registeredColliders_.begin(), registeredColliders_.end(), collider);
    if (it != registeredColliders_.end()) registeredColliders_.erase(it);

    // ICollider破棄後もIDが残らないよう、内部Colliderへの登録も解除する
    auto *nonConst = const_cast<ICollider *>(collider);
    if (auto it2D = colliderIds2D_.find(nonConst); it2D != colliderIds2D_.end()) {
        collider_.Remove2D(it2D->second);
        colliderIds2D_.erase(it2D);
    }
    if (auto it3D = colliderIds3D_.find(nonConst); it3D != colliderIds3D_.end()) {
        collider_.Remove3D(it3D->second);
        colliderIds3D_.erase(it3D);
    }
}

void SceneObjectCollider::RegisterCharacterController2D(CharacterController2D *controller) {
    if (!controller) return;
    if (std::find(characterControllers2D_.begin(), characterControllers2D_.end(), controller) != characterControllers2D_.end()) return;
    characterControllers2D_.push_back(controller);
}

void SceneObjectCollider::UnregisterCharacterController2D(const CharacterController2D *controller) {
    auto it = std::find(characterControllers2D_.begin(), characterControllers2D_.end(), controller);
    if (it != characterControllers2D_.end()) characterControllers2D_.erase(it);
}

void SceneObjectCollider::SyncRegisteredColliders() {
    for (auto *collider : registeredColliders_) {
        if (!collider) continue;

        if (collider->Is2D()) {
            auto info = collider->BuildColliderInfo2D();
            auto it = colliderIds2D_.find(collider);
            if (!info.has_value()) {
                // 常駐する形状を持たないコライダー（レイキャスト専用等）は登録しない
                if (it != colliderIds2D_.end()) {
                    collider_.Remove2D(it->second);
                    colliderIds2D_.erase(it);
                }
                continue;
            }
            info->enabled = collider->IsActive();
            info->isTrigger = collider->IsTrigger();
            info->continuousDetection = collider->IsContinuousDetection();
            info->sourceCollider = collider;
            info->onCollisionEnter = collider->GetOnCollisionEnter2D();
            info->onCollisionStay = collider->GetOnCollisionStay2D();
            info->onCollisionExit = collider->GetOnCollisionExit2D();

            if (it != colliderIds2D_.end()) {
                collider_.UpdateColliderInfo2D(it->second, *info);
            } else {
                colliderIds2D_.emplace(collider, collider_.Add(*info));
            }
        } else {
            auto info = collider->BuildColliderInfo3D();
            auto it = colliderIds3D_.find(collider);
            if (!info.has_value()) {
                if (it != colliderIds3D_.end()) {
                    collider_.Remove3D(it->second);
                    colliderIds3D_.erase(it);
                }
                continue;
            }
            info->enabled = collider->IsActive();
            info->isTrigger = collider->IsTrigger();
            info->continuousDetection = collider->IsContinuousDetection();
            info->sourceCollider = collider;
            info->onCollisionEnter = collider->GetOnCollisionEnter3D();
            info->onCollisionStay = collider->GetOnCollisionStay3D();
            info->onCollisionExit = collider->GetOnCollisionExit3D();

            if (it != colliderIds3D_.end()) {
                collider_.UpdateColliderInfo3D(it->second, *info);
            } else {
                colliderIds3D_.emplace(collider, collider_.Add(*info));
            }
        }
    }
}

void SceneObjectCollider::Update() {
    // 全Object（スクリプトを含む）のUpdate後に現在形状を同期し、CharacterController2Dが
    // 予約した移動を解決する。Transformが変わるため、その後もう一度同期してから通常の
    // 接触イベントを更新する。
    SyncRegisteredColliders();
    for (auto *controller : characterControllers2D_) {
        if (controller && controller->IsActive()) controller->ResolvePendingMove(collider_);
    }
    SyncRegisteredColliders();
    collider_.Update2D();
    // ReactPhysics3D is updated in this existing call.
    collider_.Update3D();
}

void SceneObjectCollider::Finalize() {
    collider_.Clear2D();
    collider_.Clear3D();
    colliderIds2D_.clear();
    colliderIds3D_.clear();
    characterControllers2D_.clear();
}

} // namespace KashipanEngine
