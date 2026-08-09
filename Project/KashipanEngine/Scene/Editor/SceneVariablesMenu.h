#pragma once
#ifdef USE_IMGUI
#include <string>
#include "Scene/SceneEditorContext.h"

namespace KashipanEngine {

class SceneEditor;

/// @brief シーン変数の追加・削除・編集を行うメニューウィンドウ
class SceneVariablesMenu final {
public:
    SceneVariablesMenu(Passkey<SceneEditor>, SceneEditorContext *context) : context_(context) {}
    ~SceneVariablesMenu() = default;

    void ShowImGui();

private:
    void AddVariableOfSelectedType(const std::string &key);

    /// @brief 型に応じたシーン変数の編集UI
    void ShowVariableEditor(const std::string &key, MyAny *variable);

    SceneEditorContext *context_ = nullptr;
    std::string newVariableName_;
    int newVariableType_ = 2; // 既定は Float
};

} // namespace KashipanEngine

#endif // USE_IMGUI
