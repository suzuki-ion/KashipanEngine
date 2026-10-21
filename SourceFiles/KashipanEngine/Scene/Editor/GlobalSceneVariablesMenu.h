#pragma once
#ifdef USE_IMGUI
#include <string>
#include "Scene/SceneEditorContext.h"

namespace KashipanEngine {

class SceneEditor;

/// @brief グローバルシーン変数の追加・削除・編集を行うメニューウィンドウ
/// @details シーン変数（SceneVariablesMenu）と異なり、変更内容は即座にグローバルシーン変数定義ファイル
///          （SceneManager::kDefaultGlobalSceneVariablesFilePath）へ保存される
class GlobalSceneVariablesMenu final {
public:
    GlobalSceneVariablesMenu(Passkey<SceneEditor>, SceneEditorContext *context) : context_(context) {}
    ~GlobalSceneVariablesMenu() = default;

    void ShowImGui();

private:
    void AddVariableOfSelectedType(const std::string &key);

    /// @brief 型に応じたグローバルシーン変数の編集UI
    /// @return 値が変更された場合は true
    bool ShowVariableEditor(const std::string &key, MyAny *variable);

    /// @brief 現在のグローバルシーン変数をファイルへ保存する
    void SaveToFile();

    SceneEditorContext *context_ = nullptr;
    std::string newVariableName_;
    int newVariableType_ = 2; // 既定は Float
};

} // namespace KashipanEngine

#endif // USE_IMGUI
