#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include <unordered_map>
#include "Scene/SceneEditorContext.h"

namespace KashipanEngine {

class SceneEditor;
class SceneEditorCommands;

class SceneObjectHierarchy final {
public:
    SceneObjectHierarchy(Passkey<SceneEditor>, SceneEditorContext *editorContext) : editorContext_(editorContext) {}
    ~SceneObjectHierarchy() = default;
    SceneObjectHierarchy(const SceneObjectHierarchy &) = delete;
    SceneObjectHierarchy &operator=(const SceneObjectHierarchy &) = delete;
    SceneObjectHierarchy(SceneObjectHierarchy &&) = delete;
    SceneObjectHierarchy &operator=(SceneObjectHierarchy &&) = delete;
    
    void ShowImGui();

    EmptyObject *GetSelectedObject() const { return selectedObject_; }

    /// @brief Undo/Redo用のコマンド管理を設定する
    void SetCommands(SceneEditorCommands *commands) { commands_ = commands; }
    /// @brief 選択状態をクリアする（Undo/Redoやシーンロードでオブジェクトが変わった場合用）
    void ClearSelection() {
        selectedObject_ = nullptr;
        selectedObjectIndex_ = SIZE_MAX;
    }

private:
    struct ObjectItem {
        EmptyObject *object = nullptr;
        std::string name;
        std::vector<ObjectItem> children;
        size_t depth = 0;
        size_t originalIndex = SIZE_MAX;
    };

    enum class DropPosition {
        Above,
        Inside,
        Below
    };

    struct DragDropPayload {
        // ObjectItem は毎フレーム RebuildObjectItems() で作り直される一時ツリー構造体のため、
        // フレームをまたいで保持すると（ドラッグ中に対象アイテムが描画されない等の理由で
        // ペイロードが前フレームのまま持ち越された場合）ダングリングポインタになる。
        // 実体である EmptyObject* のみを保持し、必要な情報は都度 editorContext_ 経由で取得する。
        EmptyObject *objectSource = nullptr;
        EmptyObject *objectTarget = nullptr;
        DropPosition position = DropPosition::Inside;
    };

    void RebuildObjectItems();
    void RecursivelyBuildObjectItems(EmptyObject *obj, ObjectItem &item, size_t depth);
    void ShowObjectItem(const ObjectItem &item, size_t &index);
    void ShowObjectContextMenu(EmptyObject *obj);
    void ShowHierarchyContextMenu();
    void DragAndDropObject(ObjectItem *objItem);
    void ApplyDragAndDrop();
    DropPosition DragAndDropTargetCommon();

    SceneEditorContext *editorContext_ = nullptr;
    SceneEditorCommands *commands_ = nullptr;

    std::vector<ObjectItem> objectItems_;
    std::unordered_map<EmptyObject *, std::vector<std::pair<EmptyObject *, size_t>>> objectParentMap_;
    
    size_t selectedObjectIndex_ = SIZE_MAX;
    EmptyObject *selectedObject_ = nullptr;

    DragDropPayload dragDropPayload_;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
