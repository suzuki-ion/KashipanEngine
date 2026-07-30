#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
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
    /// @brief 一括編集の対象（複数選択時の、プライマリ以外のオブジェクトとその対応コンポーネント）
    using ComponentCounterparts = std::vector<std::pair<EmptyObject *, IObjectComponent *>>;

    void ShowObjectInspector(EmptyObject *obj);
    /// @brief 選択オブジェクトがPrefabインスタンスの場合、ソースパス・Apply All/Revert All・
    ///        Override一覧＋個別Apply/Revertを表示する（対象でなければ何も表示しない）
    void ShowPrefabSection(EmptyObject *obj);
    /// @brief Prefabインスタンスの「Revert All」確認モーダル
    /// @details RevertAllはobj自体を削除・再生成するため、ShowObjectInspector実行中に
    ///          即座に呼ぶとobjがダングリングポインタになる。そのためShowImGui側で
    ///          ShowObjectInspector呼び出しが完全に終わった後に処理する
    void ShowRevertPrefabConfirmModal();
    /// @brief 複数選択時のインスペクター（全選択オブジェクトに共通するコンポーネントを表示し、編集を全てへ適用する）
    /// @param primary 値の表示元になるオブジェクト（最後に操作したオブジェクト）
    /// @param selectedObjects 選択中の全オブジェクト
    void ShowMultiObjectInspector(EmptyObject *primary, const std::unordered_set<EmptyObject *> &selectedObjects);
    /// @brief コンポーネントのパラメータ編集をUndo履歴へ積む（編集セッション単位でコアレスする）
    /// @param linkedTargets 複数選択時の一括編集対象（編集セッション開始時に変更前の状態を控える）
    void TrackComponentEdit(EmptyObject *obj, IObjectComponent *component, const JSON &before, const JSON &after,
        const ComponentCounterparts &linkedTargets = {});
    /// @brief 編集セッションが終了していたら保留中の編集をUndo履歴へ積む
    void FlushPendingComponentEdit();
    /// @brief 保留中の編集をUndo履歴へ確定する（一括編集対象があれば1つの複合コマンドにまとめる）
    void CommitPendingEdit();
    /// @brief 保留中の編集キャッシュを破棄する
    void ResetPendingEdit();
    /// @brief プライマリの編集差分（before→after）を一括編集対象の対応コンポーネントへ適用する
    static void ApplyEditToCounterparts(const JSON &before, const JSON &after, const ComponentCounterparts &counterparts);
    /// @brief オブジェクトから「同じ型のordinal番目」のコンポーネントを取得する（共通コンポーネントの対応付け用）
    static IObjectComponent *FindComponentByTypeOrdinal(EmptyObject *obj, const std::string &typeName, size_t ordinal);

    /// @brief オブジェクトが持つコンポーネントを処理優先順位（更新優先度→追加順）で並べたものを取得する
    static std::vector<IObjectComponent *> GetOrderedComponents(EmptyObject *obj);
    /// @brief コンポーネント行に対するD&Dソース/ターゲットの登録（直前に表示した項目が対象になる）
    void DragAndDropComponent(IObjectComponent *comp);
    /// @brief 保留中のコンポーネントD&Dを適用し、処理優先順位を並び順どおりに振り直す
    void ApplyComponentDragAndDrop(EmptyObject *obj);

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

    /// @brief 複数選択の一括編集セッション中の、プライマリ以外の対象とその変更前スナップショット
    struct LinkedEditTarget {
        EmptyObject *object = nullptr;
        IObjectComponent *component = nullptr;
        JSON before;
    };
    std::vector<LinkedEditTarget> editLinkedTargets_;

    // コンポーネント並び替えD&D用（Above: ターゲットの上、Below: ターゲットの下）
    enum class ComponentDropPosition { Above, Below };
    IObjectComponent *componentDragSource_ = nullptr;
    IObjectComponent *componentDragTarget_ = nullptr;
    ComponentDropPosition componentDragPosition_ = ComponentDropPosition::Above;

    // Prefabインスタンスの「Revert All」確認モーダル用
    bool isRevertPrefabConfirmRequested_ = false;
    EmptyObject *pendingRevertPrefabTarget_ = nullptr;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
