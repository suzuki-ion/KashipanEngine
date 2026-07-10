#pragma once
#include "Scene/Components/SceneComponentHeader.h"

class asIScriptEngine;

namespace KashipanEngine {

/// @brief シーン内でAngelScriptを実行するための共有スクリプトエンジンを管理するシーンコンポーネント
/// @details ScriptComponent はこのコンポーネントを介して asIScriptEngine を取得する。
///          シーンに未追加の場合は ScriptComponent::Initialize 時に自動で追加される。
///          現時点ではエンジン側の機能をスクリプトへ公開するための型/関数登録は行っていない。
class SceneScriptEngine final : public ISceneComponent {
public:
    SCENE_COMPONENT_CONSTRUCTOR(SceneScriptEngine, 1, )
    COMPONENT_CATEGORY("Script")
    ~SceneScriptEngine() override;

    std::unique_ptr<ISceneComponent> Clone() const override {
        return std::make_unique<SceneScriptEngine>();
    }

    /// @brief 共有スクリプトエンジンを取得（未初期化の場合は nullptr）
    asIScriptEngine *GetEngine() const noexcept { return engine_; }

protected:
    void Initialize() override;
    void Finalize() override;

#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif

private:
    asIScriptEngine *engine_ = nullptr;
};

REGISTER_COMPONENT_SCENE(SceneScriptEngine)

} // namespace KashipanEngine
