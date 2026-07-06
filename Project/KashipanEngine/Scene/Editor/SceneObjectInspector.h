#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include <string>
#include "Utilities/ImGuiCustom.h"
#include "Scene/SceneEditorContext.h"
#include "Scene/Editor/SceneObjectHierarchy.h"

namespace KashipanEngine {

class SceneEditor;
class SceneEditorCommands;

class SceneObjectInspector final {
public:
    SceneObjectInspector(Passkey<SceneEditor>, SceneEditorContext *context, SceneObjectHierarchy *objectHierarchy)
        : context_(context), objectHierarchy_(objectHierarchy) {}
    ~SceneObjectInspector() = default;

    /// @brief Undo/Redo用のコマンド管理を設定する
    void SetCommands(SceneEditorCommands *commands) { commands_ = commands; }

    void ShowImGui();

private:
    void ShowObjectInspector(EmptyObject *obj);
    /// @brief コンポーネントのパラメータ編集をUndo履歴へ積む（編集セッション単位でコアレスする）
    void TrackComponentEdit(EmptyObject *obj, IObjectComponent *component, const JSON &before, const JSON &after);
    /// @brief 編集セッションが終了していたら保留中の編集をUndo履歴へ積む
    void FlushPendingComponentEdit();

    SceneEditorContext *context_ = nullptr;
    SceneObjectHierarchy *objectHierarchy_ = nullptr;
    SceneEditorCommands *commands_ = nullptr;

    // 名前編集のUndo用（編集開始時の名前を保持）
    std::string nameBeforeEdit_;

    // コンポーネントパラメータ編集のコアレス用
    bool hasPendingEdit_ = false;
    EmptyObject *editObject_ = nullptr;
    IObjectComponent *editComponent_ = nullptr;
    JSON editBefore_;
    JSON editAfter_;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
