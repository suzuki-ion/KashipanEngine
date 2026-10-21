#pragma once
#include <vector>

#include "Scene/Components/SceneComponentHeader.h"

namespace KashipanEngine {

class Shake;

/// @brief Shakeコンポーネント（処理タイミング=DeferredEnd）のTransformへの適用を、
///        全オブジェクトのUpdate/衝突解決が終わった後にまとめて行うシーンコンポーネント
/// @details 更新優先度を非常に大きい値にすることで、他の全てのシーンコンポーネント
///          （SceneObjectCollider等）より後に実行されることを保証する。これにより、
///          同フレーム中に他のスクリプト・物理押し戻し等がTransformを書き換えても、
///          それらは常に「揺れていない」素のTransformに対して行われ、Shakeによる
///          オフセットは最後に一度だけ・確実に上書きされずに乗る
class SceneShakeApplier final : public ISceneComponent {
public:
    SCENE_COMPONENT_CONSTRUCTOR(SceneShakeApplier, 1, SetUpdatePriority(2000);)
    COMPONENT_CATEGORY("Utility")
    ~SceneShakeApplier() override = default;

    std::unique_ptr<ISceneComponent> Clone() const override {
        return std::make_unique<SceneShakeApplier>();
    }

    void RegisterShake(Shake *shake);
    void UnregisterShake(const Shake *shake);

protected:
    void Update() override;

private:
    std::vector<Shake *> shakes_;
};

REGISTER_COMPONENT_SCENE(SceneShakeApplier)

} // namespace KashipanEngine
