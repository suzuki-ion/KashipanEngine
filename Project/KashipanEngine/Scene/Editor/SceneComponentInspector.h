#pragma once
#ifdef USE_IMGUI
#include "Scene/SceneEditorContext.h"

namespace KashipanEngine {

class SceneEditor;
class SceneEditorCommands;

/// @brief シーンコンポーネントのインスペクターウィンドウ
class SceneComponentInspector final {
public:
    SceneComponentInspector(Passkey<SceneEditor>, SceneEditorContext *context) : context_(context) {}
    ~SceneComponentInspector() = default;

    /// @brief Undo/Redo へ積むためのコマンド管理（GameEventRouter のイベント定義の編集を記録する）
    void SetCommands(SceneEditorCommands *commands) { commands_ = commands; }
    void ShowImGui();

private:
    SceneEditorContext *context_ = nullptr;
    SceneEditorCommands *commands_ = nullptr;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
