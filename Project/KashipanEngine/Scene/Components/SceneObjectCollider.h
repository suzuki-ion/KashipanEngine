#pragma once

#include <unordered_map>
#include <vector>

#include "Objects/Collision/Collider.h"
#include "Scene/Components/SceneComponentHeader.h"

namespace KashipanEngine {

class ICollider;

/// @brief シーン内のICollider派生コンポーネントを収集し、実際の当たり判定処理(Collider)へ橋渡しするシーンコンポーネント
/// @details ICollider::Initialize/FinalizeからRegister/Unregisterされる。
///          登録はポインタのみで行われ、以後は毎フレームこのコンポーネント側が
///          各IColliderの現在の状態（形状・トリガー設定・コールバック等）を読み取って
///          内部のColliderへ反映するため、ICollider側で変更があっても再登録は不要。
class SceneObjectCollider final : public ISceneComponent {
public:
    SCENE_COMPONENT_CONSTRUCTOR(SceneObjectCollider, 1, )
    COMPONENT_CATEGORY("Collision")
    ~SceneObjectCollider() override = default;

    std::unique_ptr<ISceneComponent> Clone() const override {
        return std::make_unique<SceneObjectCollider>();
    }

    Collider *GetCollider() { return &collider_; }
    const Collider *GetCollider() const { return &collider_; }

    /// @brief ICollider派生コンポーネントを登録する（ポインタ登録。以後は毎フレーム自動で最新状態が反映される）
    void RegisterCollider(ICollider *collider);
    /// @brief ICollider派生コンポーネントの登録を解除する
    void UnregisterCollider(const ICollider *collider);

    /// @brief 登録済みICollider派生コンポーネント一覧を取得する（デバッグシーンビューでの可視化等に使用）
    const std::vector<ICollider *> &GetRegisteredColliders() const { return registeredColliders_; }

protected:
    void Update() override;
    void Finalize() override;

private:
    /// @brief 登録済みICollider群の現在の状態をColliderへ反映する
    void SyncRegisteredColliders();

    Collider collider_{};
    std::vector<ICollider *> registeredColliders_;
    std::unordered_map<ICollider *, Collider::ColliderID> colliderIds2D_;
    std::unordered_map<ICollider *, Collider::ColliderID> colliderIds3D_;
};

REGISTER_COMPONENT_SCENE(SceneObjectCollider)

} // namespace KashipanEngine
