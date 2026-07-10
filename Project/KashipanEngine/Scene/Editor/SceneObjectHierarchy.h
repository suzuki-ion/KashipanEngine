#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include <unordered_map>
#include <unordered_set>
#include "Scene/SceneEditorContext.h"
#include "Scene/Editor/SceneEditorCommands.h"

namespace KashipanEngine {

class SceneEditor;

class SceneObjectHierarchy final {
public:
    SceneObjectHierarchy(Passkey<SceneEditor>, SceneEditorContext *editorContext) : editorContext_(editorContext) {}
    ~SceneObjectHierarchy() = default;
    SceneObjectHierarchy(const SceneObjectHierarchy &) = delete;
    SceneObjectHierarchy &operator=(const SceneObjectHierarchy &) = delete;
    SceneObjectHierarchy(SceneObjectHierarchy &&) = delete;
    SceneObjectHierarchy &operator=(SceneObjectHierarchy &&) = delete;
    
    void ShowImGui();

    /// @brief 選択中オブジェクト（複数選択時は最後に操作したオブジェクト。インスペクター等の単一対象表示用）
    EmptyObject *GetSelectedObject() const { return selectedObject_; }
    /// @brief 選択中の全オブジェクト（Shift範囲選択・Ctrl個別選択の結果）
    const std::unordered_set<EmptyObject *> &GetSelectedObjects() const { return selectedObjects_; }

    /// @brief Undo/Redo用のコマンド管理を設定する
    void SetCommands(SceneEditorCommands *commands) { commands_ = commands; }
    /// @brief 選択状態をクリアする（Undo/Redoやシーンロードでオブジェクトが変わった場合用）
    void ClearSelection() {
        selectedObject_ = nullptr;
        selectedObjects_.clear();
        selectionAnchorObject_ = nullptr;
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
    /// @brief オブジェクト作成メニューの中身（グループ分けされたテンプレート一覧）を表示する
    /// @param referenceObject 基準オブジェクト（asChild=falseの場合は兄弟として作成。nullptrならルート直下）
    /// @param asChild trueの場合、referenceObjectの子として作成する（referenceObjectは非nullである必要がある）
    void ShowCreateObjectMenu(EmptyObject *referenceObject, bool asChild);
    /// @brief テンプレート化されたオブジェクト（指定コンポーネント一式を持つ）を作成する
    /// @param objectName 作成するオブジェクトの名前
    /// @param componentTypes 追加するコンポーネントの型名一覧（登録済みの型名文字列と一致させること）
    /// @param referenceObject 基準オブジェクト（asChild=falseの場合は兄弟として作成。nullptrならルート直下）
    /// @param asChild trueの場合、referenceObjectの子として作成する
    void CreateTemplateObject(const std::string &objectName, const std::vector<std::string> &componentTypes,
        EmptyObject *referenceObject, bool asChild);
    void DragAndDropObject(ObjectItem *objItem);
    void ApplyDragAndDrop();
    DropPosition DragAndDropTargetCommon();

    /// @brief Shift範囲選択の確定処理（ツリー全体の表示順が確定した後に呼ぶ必要がある）
    void ApplyPendingRangeSelect();

    /// @brief Ctrl+C/Ctrl+V/Ctrl+Shift+V/Ctrl+Dのショートカット操作を処理する
    void HandleKeyboardShortcuts();
    /// @brief 現在の選択集合から、他の選択オブジェクトの子孫であるものを除いたトップレベルの一覧を返す
    /// @details 親と子の両方が選択されている場合、親の削除/複製で子孫も連動するため二重処理を避ける
    std::vector<EmptyObject *> GetSelectionRoots() const;
    /// @brief 指定オブジェクト群（とそれぞれの子孫）をクリップボードへコピーする
    void CopyObjects(const std::vector<EmptyObject *> &objs);
    /// @brief クリップボードの内容を指定位置へ貼り付ける（attachParentがnullptrの場合はルート直下）
    void PasteObject(EmptyObject *attachParent, size_t insertIndex);
    /// @brief 指定オブジェクト群（とそれぞれの子孫）を複製する（各オブジェクトは元と同じ親を維持する）
    void CloneObjects(const std::vector<EmptyObject *> &objs);
    /// @brief 指定オブジェクト群を削除する（複数の場合はまとめて1つのUndo操作にする）
    void DeleteObjects(const std::vector<EmptyObject *> &objs);
    /// @brief 指定オブジェクトを根とする部分木のJSONスナップショットを収集する（pre-order）
    void CollectSubtreeNodes(EmptyObject *obj, int parentIndex, std::vector<PasteObjectCommand::Node> &out) const;
    /// @brief 貼り付け/複製コマンドを実行し、成功したら生成された全ルートオブジェクトを選択状態にする
    void ExecutePasteCommand(std::unique_ptr<PasteObjectCommand> command);

    SceneEditorContext *editorContext_ = nullptr;
    SceneEditorCommands *commands_ = nullptr;

    std::vector<ObjectItem> objectItems_;
    std::unordered_map<EmptyObject *, std::vector<std::pair<EmptyObject *, size_t>>> objectParentMap_;

    // 複数選択の状態。selectedObject_ は最後に操作したオブジェクト（インスペクター/ギズモ等、
    // 単一対象を要求する既存の呼び出し元との後方互換用）で、常に selectedObjects_ に含まれる
    // （selectedObjects_ が空の場合のみ nullptr）。
    std::unordered_set<EmptyObject *> selectedObjects_;
    EmptyObject *selectedObject_ = nullptr;
    // Shift範囲選択の起点。修飾キー無し/Ctrlクリック時に更新し、Shiftクリックでは維持する
    // （連続Shiftクリックで同じ起点から範囲を再計算できるようにするため）。
    EmptyObject *selectionAnchorObject_ = nullptr;
    // Shiftクリックされた対象。ツリー全体の表示順（visibleOrderThisFrame_）が確定してから
    // ApplyPendingRangeSelect() で範囲を確定するため、クリック時点では要求のみ記録する。
    EmptyObject *pendingRangeTarget_ = nullptr;
    // このフレームで実際に表示された行の順序（Shift範囲選択の範囲計算に使う）
    std::vector<EmptyObject *> visibleOrderThisFrame_;

    DragDropPayload dragDropPayload_;

    // コピー＆ペースト用クリップボード（部分木のJSONスナップショット。pre-order、先頭が根）
    std::vector<PasteObjectCommand::Node> clipboardNodes_;
    std::string clipboardRootName_;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
