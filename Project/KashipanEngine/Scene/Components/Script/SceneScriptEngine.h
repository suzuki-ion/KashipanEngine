#pragma once
#include <string>
#include <vector>

#include "Scene/Components/SceneComponentHeader.h"

class asIScriptEngine;
class asIScriptContext;

namespace KashipanEngine {

class AngelScriptDebugServer;

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

    /// @brief コンテキストをVS Code用DAPデバッガーへ接続する
    /// @details Releaseビルドまたはデバッグサーバーを開始できなかった場合は何もしない
    void AttachDebugger(asIScriptContext *context) const;

    /// @brief スクリプトビルド時のコンパイルメッセージの収集を開始する
    /// @details 収集中もログへの出力は行われる。ScriptComponentがビルドエラーの詳細表示に使用する
    void BeginMessageCapture();
    /// @brief メッセージ収集を終了し、収集したメッセージを取得する
    std::vector<std::string> EndMessageCapture();

protected:
    void Initialize() override;
    void Finalize() override;

#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif

private:
    asIScriptEngine *engine_ = nullptr;
    /// @brief プロセス共通DAPサーバーへの非所有ポインター
    AngelScriptDebugServer *debugServer_ = nullptr;
    /// @brief メッセージ収集用バッファ（BeginMessageCapture～EndMessageCaptureの間だけ使用）
    std::vector<std::string> messageCaptureBuffer_;
};

REGISTER_COMPONENT_SCENE(SceneScriptEngine)

} // namespace KashipanEngine
