#include "ICollider.h"

#include "Objects/Components/Transform.h"
#include "Objects/ObjectContext.h"
#include "Scene/Components/SceneObjectCollider.h"
#include "Scene/SceneContext.h"

namespace KashipanEngine {

void ICollider::Initialize() {
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
    auto *sceneContext = GetOwnerSceneContext();
    auto *sceneObjectCollider = sceneContext ? sceneContext->GetComponent<SceneObjectCollider>() : nullptr;
    if (sceneObjectCollider) {
        sceneObjectCollider->UnregisterCollider(this);
    }
}

Vector3 ICollider::GetOwnerWorldPosition() const {
    auto *objectContext = GetOwnerObjectContext();
    auto *transform = objectContext ? objectContext->GetComponent<Transform>() : nullptr;
    if (!transform) return Vector3(0.0f, 0.0f, 0.0f);
    const Matrix4x4 &world = transform->GetWorldMatrix();
    return Vector3(world.m[3][0], world.m[3][1], world.m[3][2]);
}

Vector3 ICollider::GetSyncedOwnerPosition() const {
    Vector3 pos = GetOwnerWorldPosition();
    if (!syncPosition_[0]) pos.x = 0.0f;
    if (!syncPosition_[1]) pos.y = 0.0f;
    if (!syncPosition_[2]) pos.z = 0.0f;
    return pos;
}

Vector3 ICollider::GetSyncedOwnerRotationEuler() const {
    Vector3 rot{ 0.0f, 0.0f, 0.0f };
    auto *objectContext = GetOwnerObjectContext();
    auto *transform = objectContext ? objectContext->GetComponent<Transform>() : nullptr;
    if (transform) {
        Quaternion worldRotate = transform->GetWorldRotateQuaternion();
        rot = worldRotate.MakeEuler();
    }
    if (!syncRotation_[0]) rot.x = 0.0f;
    if (!syncRotation_[1]) rot.y = 0.0f;
    if (!syncRotation_[2]) rot.z = 0.0f;
    return rot;
}

Quaternion ICollider::GetSyncedOwnerRotation() const {
    // 全軸同期の場合はTransformのワールド回転クォータニオンをそのまま使う。
    // オイラー角を経由すると、分解(Quaternion::MakeEuler)と再構成(Quaternion::MakeRotateEuler)の
    // 回転順が一致しないため、ImGuizmoのようにクォータニオンで直接回転を設定した場合に
    // 軸ごと反転したような結果になってしまう（インスペクターのオイラー角入力経由だと、
    // 元々MakeRotateEulerで生成された値を再度MakeRotateEulerに通すだけなので問題が起きない）。
    if (syncRotation_[0] && syncRotation_[1] && syncRotation_[2]) {
        auto *objectContext = GetOwnerObjectContext();
        auto *transform = objectContext ? objectContext->GetComponent<Transform>() : nullptr;
        return transform ? transform->GetWorldRotateQuaternion() : Quaternion::Identity();
    }
    if (!syncRotation_[0] && !syncRotation_[1] && !syncRotation_[2]) {
        return Quaternion::Identity();
    }
    return Quaternion::MakeRotateEuler(GetSyncedOwnerRotationEuler());
}

Vector3 ICollider::GetSyncedOwnerScale() const {
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
