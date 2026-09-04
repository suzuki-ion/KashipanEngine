#include "ICollider.h"

#include "Debug/Logger.h"
#include "Objects/Components/Transform.h"
#include "Objects/ObjectContext.h"
#include "Scene/Components/SceneObjectCollider.h"
#include "Scene/SceneContext.h"

namespace KashipanEngine {

void ICollider::Initialize() {
    LogScope scope;
    auto *sceneContext = GetOwnerSceneContext();
    if (!sceneContext) return;
    auto *sceneObjectCollider = sceneContext->GetComponent<SceneObjectCollider>();
    if (!sceneObjectCollider) {
        sceneObjectCollider = sceneContext->AddComponent<SceneObjectCollider>();
    }
    if (sceneObjectCollider) {
        sceneObjectCollider->RegisterCollider(this);
    }
}

void ICollider::Finalize() {
    LogScope scope;
    auto *sceneContext = GetOwnerSceneContext();
    auto *sceneObjectCollider = sceneContext ? sceneContext->GetComponent<SceneObjectCollider>() : nullptr;
    if (sceneObjectCollider) {
        sceneObjectCollider->UnregisterCollider(this);
    }
}

Vector3 ICollider::GetOwnerWorldPosition() const {
    LogScope scope;
    auto *objectContext = GetOwnerObjectContext();
    auto *transform = objectContext ? objectContext->GetComponent<Transform>() : nullptr;
    if (!transform) return Vector3(0.0f, 0.0f, 0.0f);
    const Matrix4x4 &world = transform->GetWorldMatrix();
    return Vector3(world.m[3][0], world.m[3][1], world.m[3][2]);
}

Vector3 ICollider::GetSyncedOwnerPosition() const {
    LogScope scope;
    Vector3 pos = GetOwnerWorldPosition();
    if (!syncPosition_[0]) pos.x = 0.0f;
    if (!syncPosition_[1]) pos.y = 0.0f;
    if (!syncPosition_[2]) pos.z = 0.0f;
    return pos;
}

Vector3 ICollider::GetSyncedOwnerRotationEuler() const {
    LogScope scope;
    // 2D系コライダーがZ角度のみを取り出すために使う。回転は全軸同期/非同期の二択のため、
    // 非同期時は分解すら行わずゼロを返す。
    if (!syncRotation_) return Vector3{ 0.0f, 0.0f, 0.0f };
    return GetSyncedOwnerRotation().MakeEuler();
}

Quaternion ICollider::GetSyncedOwnerRotation() const {
    LogScope scope;
    // 回転の部分同期（軸ごとにオイラー角を分解して一部をゼロにする）は、分解順序への依存と
    // ジンバルロックにより反対向きの回転や不連続なジャンプを引き起こすためサポートしない。
    // 全軸同期時はTransformのワールド回転クォータニオンをそのまま使うため、
    // オイラー角の往復（分解→再構成）が発生せずこの問題は起きない。
    if (!syncRotation_) return Quaternion::Identity();
    auto *objectContext = GetOwnerObjectContext();
    auto *transform = objectContext ? objectContext->GetComponent<Transform>() : nullptr;
    return transform ? transform->GetWorldRotateQuaternion() : Quaternion::Identity();
}

Vector3 ICollider::GetSyncedOwnerScale() const {
    LogScope scope;
    Vector3 scale{ 1.0f, 1.0f, 1.0f };
    auto *objectContext = GetOwnerObjectContext();
    auto *transform = objectContext ? objectContext->GetComponent<Transform>() : nullptr;
    if (transform) scale = transform->GetScale();
    if (!syncScale_[0]) scale.x = 1.0f;
    if (!syncScale_[1]) scale.y = 1.0f;
    if (!syncScale_[2]) scale.z = 1.0f;
    return scale;
}

} // namespace KashipanEngine
