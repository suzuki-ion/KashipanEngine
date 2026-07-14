#pragma once
#include <vector>

#include "Scene/Components/SceneComponentHeader.h"

namespace KashipanEngine {

class PreTransform;

/// @brief シーン内のPreTransformコンポーネントを収集し、毎フレーム最後に前フレーム値を更新するシーンコンポーネント
/// @details PreTransform::Initialize/FinalizeからRegister/Unregisterされる。
///          このコンポーネントの更新優先度はSceneRenderer（1000）より大きい値にしてあるため、
///          そのフレームの全オブジェクトの更新（Transformを動かすVelocity/Rotation/ScriptComponent等）と
///          全シーンコンポーネントの更新（描画を含む）が完了した後に、現在のTransformの値を
///          「前フレームの値」として書き込む。そのため、レンダラーはこのフレームの描画中は
///          まだ前フレームの値を参照でき、次フレームに向けてここで値が更新される。
class ScenePreTransform final : public ISceneComponent {
public:
    SCENE_COMPONENT_CONSTRUCTOR(ScenePreTransform, 1, SetUpdatePriority(1001);)
    COMPONENT_CATEGORY("Physics")
    ~ScenePreTransform() override = default;

    std::unique_ptr<ISceneComponent> Clone() const override {
        return std::make_unique<ScenePreTransform>();
    }

    /// @brief PreTransformコンポーネントを登録する（ポインタ登録。以後は毎フレーム自動で前フレーム値が更新される）
    void RegisterPreTransform(PreTransform *preTransform);
    /// @brief PreTransformコンポーネントの登録を解除する
    void UnregisterPreTransform(const PreTransform *preTransform);

protected:
    void Update() override;
    void Finalize() override;

private:
    std::vector<PreTransform *> registeredPreTransforms_;
};

REGISTER_COMPONENT_SCENE(ScenePreTransform)

} // namespace KashipanEngine
