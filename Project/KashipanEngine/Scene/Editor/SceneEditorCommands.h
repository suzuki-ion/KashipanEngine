#pragma once
#ifdef USE_IMGUI
#include <memory>
#include <string>
#include <vector>

#include "Debug/Logger.h"
#include "Scene/SceneEditorContext.h"
#include "Objects/ComponentRef.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

class SceneEditor;

//==================================================
// エディター操作コマンドの基底
//==================================================

/// @brief シーンエディター操作コマンドのインターフェース（コマンドパターン）
class IEditorCommand {
public:
    virtual ~IEditorCommand() = default;
    /// @brief 操作を実行する（Redoでも呼ばれる）
    virtual bool Execute(SceneEditorContext *context) = 0;
    /// @brief 操作を取り消す
    virtual bool Undo(SceneEditorContext *context) = 0;
    /// @brief 操作名（Undo/Redoメニュー表示用）
    virtual std::string GetName() const = 0;
};

//==================================================
// オブジェクト操作コマンド
//==================================================

/// @brief オブジェクト生成コマンド
class CreateObjectCommand final : public IEditorCommand {
public:
    explicit CreateObjectCommand(const std::string &name, size_t index = MAXSIZE_T)
        : name_(name), objectID_(true), index_(index) {}

    bool Execute(SceneEditorContext *context) override;
    bool Undo(SceneEditorContext *context) override;
    std::string GetName() const override { return Translation("editor.command.createobject") + name_; }

    const UUID128 &GetObjectID() const noexcept { return objectID_; }

private:
    std::string name_;
    UUID128 objectID_;
    size_t index_ = MAXSIZE_T;
};

/// @brief オブジェクトを兄弟として作成するコマンド（参照オブジェクトと同じ親・指定インデックス）
class CreateSiblingObjectCommand final : public IEditorCommand {
public:
    CreateSiblingObjectCommand(const std::string &name, EmptyObject *referenceObject, size_t index)
        : name_(name), objectID_(true),
          referenceObjectID_(referenceObject ? referenceObject->GetObjectID() : UUID128()),
          index_(index) {}

    bool Execute(SceneEditorContext *context) override;
    bool Undo(SceneEditorContext *context) override;
    std::string GetName() const override { return Translation("editor.command.createobject") + name_; }

    const UUID128 &GetObjectID() const noexcept { return objectID_; }

private:
    std::string name_;
    UUID128 objectID_;
    UUID128 referenceObjectID_;
    size_t index_ = MAXSIZE_T;
};

/// @brief オブジェクトを指定オブジェクトの子として末尾に作成するコマンド
class CreateChildObjectCommand final : public IEditorCommand {
public:
    CreateChildObjectCommand(const std::string &name, EmptyObject *parentObject)
        : name_(name), objectID_(true),
          parentObjectID_(parentObject ? parentObject->GetObjectID() : UUID128()) {}

    bool Execute(SceneEditorContext *context) override;
    bool Undo(SceneEditorContext *context) override;
    std::string GetName() const override { return Translation("editor.command.createchildobject") + name_; }

    const UUID128 &GetObjectID() const noexcept { return objectID_; }

private:
    std::string name_;
    UUID128 objectID_;
    UUID128 parentObjectID_;
};

/// @brief オブジェクト（とその子孫）をJSONスナップショットから貼り付け／複製するコマンド
/// @details 各ノードのJSONはコマンド生成時点で新しいobjectID・親子関係（サブツリー内の参照）を
///          反映済みの状態で渡される想定（Copy/Paste/Clone側で構築する）。
class PasteObjectCommand final : public IEditorCommand {
public:
    struct Node {
        JSON json;
        int parentIndexInSubtree = -1;
    };

    // attachParent: 部分木の根（parentIndexInSubtree<0のノード）を接続する先。
    //   preserveOriginalRootParentがtrueの場合は無視され、各ルートノードのJSONに残された
    //   元の親参照（Transformの"parent"）がロード時にそのまま解決される（＝複製時に使用。
    //   複数オブジェクトを同時に複製する際、それぞれ元と異なる親を持っていても正しく再現できる）。
    // nodesは複数の独立したルート（parentIndexInSubtree<0のノードが複数）を含んでよい
    //   （複数選択でのコピー/複製に対応するため）。
    PasteObjectCommand(std::vector<Node> nodes, EmptyObject *attachParent, size_t insertIndex,
        const std::string &rootName, const std::string &commandName, bool preserveOriginalRootParent = false)
        : nodes_(std::move(nodes)),
          attachParentID_(attachParent ? attachParent->GetObjectID() : UUID128()),
          insertIndex_(insertIndex), rootName_(rootName), commandName_(commandName),
          preserveOriginalRootParent_(preserveOriginalRootParent) {}

    bool Execute(SceneEditorContext *context) override;
    bool Undo(SceneEditorContext *context) override;
    std::string GetName() const override { return commandName_ + ": " + rootName_; }

    EmptyObject *GetRootObject(SceneEditorContext *context) const {
        LogScope scope;
        if (nodes_.empty()) return nullptr;
        return context->GetSceneObject(UUID128(nodes_.front().json.value("objectID", std::string{})));
    }
    /// @brief 生成された部分木の根を全て取得する（複数選択でのコピー/複製に対応するため複数返りうる）
    std::vector<EmptyObject *> GetRootObjects(SceneEditorContext *context) const {
        LogScope scope;
        std::vector<EmptyObject *> result;
        for (const auto &node : nodes_) {
            if (node.parentIndexInSubtree >= 0) continue;
            if (auto *obj = context->GetSceneObject(UUID128(node.json.value("objectID", std::string{})))) {
                result.push_back(obj);
            }
        }
        return result;
    }

private:
    std::vector<Node> nodes_;
    UUID128 attachParentID_;
    size_t insertIndex_ = MAXSIZE_T;
    std::string rootName_;
    std::string commandName_;
    bool preserveOriginalRootParent_ = false;
};

/// @brief オブジェクト削除コマンド
class DeleteObjectCommand final : public IEditorCommand {
public:
    explicit DeleteObjectCommand(EmptyObject *obj)
        : objectID_(obj ? obj->GetObjectID() : UUID128()), name_(obj ? obj->GetName() : "") {}

    bool Execute(SceneEditorContext *context) override;
    bool Undo(SceneEditorContext *context) override;
    std::string GetName() const override { return Translation("editor.command.deleteobject") + name_; }

private:
    UUID128 objectID_;
    std::string name_;
    // 削除対象とその全子孫のスナップショット（pre-order、先頭が削除対象自身）。
    // 子孫は元のobjectID・親参照のままにしておき、Undo時に元の親子関係が自動的に復元されるようにする。
    std::vector<PasteObjectCommand::Node> snapshot_;
    size_t index_ = MAXSIZE_T;
};

/// @brief オブジェクト移動（並び替え）コマンド
class MoveObjectCommand final : public IEditorCommand {
public:
    MoveObjectCommand(EmptyObject *obj, size_t newIndex)
        : objectID_(obj ? obj->GetObjectID() : UUID128()), newIndex_(newIndex) {}
    /// @brief 適用済みの移動を記録する用（PushExecuted と併用）
    MoveObjectCommand(EmptyObject *obj, size_t oldIndex, size_t newIndex)
        : objectID_(obj ? obj->GetObjectID() : UUID128()), newIndex_(newIndex), oldIndex_(oldIndex) {}

    bool Execute(SceneEditorContext *context) override;
    bool Undo(SceneEditorContext *context) override;
    std::string GetName() const override { return Translation("editor.command.moveobject"); }

private:
    UUID128 objectID_;
    size_t newIndex_ = MAXSIZE_T;
    size_t oldIndex_ = MAXSIZE_T;
};

/// @brief オブジェクトのプロパティ（名前・アクティブ状態）変更コマンド
class ObjectPropertyCommand final : public IEditorCommand {
public:
    ObjectPropertyCommand(EmptyObject *obj,
        const std::string &oldName, const std::string &newName,
        bool oldActive, bool newActive)
        : objectID_(obj ? obj->GetObjectID() : UUID128()),
          oldName_(oldName), newName_(newName),
          oldActive_(oldActive), newActive_(newActive) {}

    bool Execute(SceneEditorContext *context) override;
    bool Undo(SceneEditorContext *context) override;
    std::string GetName() const override { return Translation("editor.command.editobject") + newName_; }

private:
    UUID128 objectID_;
    std::string oldName_;
    std::string newName_;
    bool oldActive_ = true;
    bool newActive_ = true;
};

/// @brief Prefab同期用のオブジェクト基本プロパティ一括変更コマンド
/// @details name/tag/isActive/editorOnlyをJSONスナップショットから適用し、Undo可能にする。
class ObjectStateCommand final : public IEditorCommand {
public:
    ObjectStateCommand(EmptyObject *obj, JSON before, JSON after)
        : objectID_(obj ? obj->GetObjectID() : UUID128()),
          before_(std::move(before)), after_(std::move(after)) {}

    bool Execute(SceneEditorContext *context) override;
    bool Undo(SceneEditorContext *context) override;
    std::string GetName() const override { return Translation("editor.command.syncobjectproperties"); }

private:
    static bool Apply(SceneEditorContext *context, const UUID128 &objectID, const JSON &state);

    UUID128 objectID_;
    JSON before_;
    JSON after_;
};

//==================================================
// コンポーネント操作コマンド
//==================================================

/// @brief コンポーネント追加コマンド
class AddComponentCommand final : public IEditorCommand {
public:
    AddComponentCommand(EmptyObject *obj, const std::string &componentType)
        : objectID_(obj ? obj->GetObjectID() : UUID128()), componentType_(componentType) {}
    /// @brief 対象オブジェクトがまだ生成されていない場合用（同一CompositeCommand内で、
    ///        先行するオブジェクト生成コマンドが割り当て済みのobjectIDを直接指定する）
    AddComponentCommand(const UUID128 &objectID, const std::string &componentType)
        : objectID_(objectID), componentType_(componentType) {}
    /// @brief 追加と同時に特定の値へ初期化したい場合に使う（Prefab同期での「追加」伝播など）。
    ///        Execute時にこのJSONがロードされる（Redo時の復元と同じ既存の仕組みをそのまま利用する）
    AddComponentCommand(EmptyObject *obj, const std::string &componentType, JSON initialState)
        : objectID_(obj ? obj->GetObjectID() : UUID128()), componentType_(componentType), state_(std::move(initialState)) {}
    /// @brief 対象オブジェクトがまだ生成されていない場合用（同一CompositeCommand内で、先行する
    ///        オブジェクト生成コマンドが割り当て済みのobjectIDを直接指定する）＋初期値JSON指定版
    AddComponentCommand(const UUID128 &objectID, const std::string &componentType, JSON initialState)
        : objectID_(objectID), componentType_(componentType), state_(std::move(initialState)) {}

    bool Execute(SceneEditorContext *context) override;
    bool Undo(SceneEditorContext *context) override;
    std::string GetName() const override { return Translation("editor.command.addcomponent") + componentType_; }

private:
    UUID128 objectID_;
    std::string componentType_;
    // Undo/Redoスタック上で複数フレームにまたがって保持されるため、生ポインタではなく
    // ComponentRef（UUID+addedID）を正として保持する（プールのスロット再利用対策）
    ComponentRef componentRef_;
    JSON state_;
};

/// @brief コンポーネント削除コマンド
class RemoveComponentCommand final : public IEditorCommand {
public:
    RemoveComponentCommand(EmptyObject *obj, IObjectComponent *component)
        : objectID_(obj ? obj->GetObjectID() : UUID128()), componentRef_(component ? component->GetComponentRef() : ComponentRef{}),
          componentType_(component ? component->GetComponentType() : "") {}

    bool Execute(SceneEditorContext *context) override;
    bool Undo(SceneEditorContext *context) override;
    std::string GetName() const override { return Translation("editor.command.removecomponent") + componentType_; }

private:
    UUID128 objectID_;
    // Undo/Redoスタック上で複数フレームにまたがって保持されるため、生ポインタではなく
    // ComponentRef（UUID+addedID）を正として保持する（プールのスロット再利用対策）
    ComponentRef componentRef_;
    std::string componentType_;
    JSON snapshot_;
};

/// @brief コンポーネントのパラメータ変更コマンド（変更前後のJSONスナップショット）
/// @details 変更適用済みの状態で積まれる想定（PushExecutedを使用する）
class ComponentEditCommand final : public IEditorCommand {
public:
    ComponentEditCommand(EmptyObject *obj, IObjectComponent *component, JSON before, JSON after)
        : objectID_(obj ? obj->GetObjectID() : UUID128()), componentRef_(component ? component->GetComponentRef() : ComponentRef{}),
          componentType_(component ? component->GetComponentType() : ""),
          before_(std::move(before)), after_(std::move(after)) {}

    bool Execute(SceneEditorContext *context) override;
    bool Undo(SceneEditorContext *context) override;
    std::string GetName() const override { return Translation("editor.command.editcomponent") + componentType_; }

private:
    UUID128 objectID_;
    // Undo/Redoスタック上で複数フレームにまたがって保持されるため、生ポインタではなく
    // ComponentRef（UUID+addedID）を正として保持する（プールのスロット再利用対策）
    ComponentRef componentRef_;
    std::string componentType_;
    JSON before_;
    JSON after_;
};

//==================================================
// 複合コマンド
//==================================================

/// @brief 複数コマンドをひとつの操作として扱う複合コマンド
class CompositeCommand final : public IEditorCommand {
public:
    explicit CompositeCommand(const std::string &name) : name_(name) {}

    void AddCommand(std::unique_ptr<IEditorCommand> command) {
        LogScope scope;
        if (command) commands_.push_back(std::move(command));
    }
    bool IsEmpty() const noexcept { return commands_.empty(); }

    bool Execute(SceneEditorContext *context) override;
    bool Undo(SceneEditorContext *context) override;
    std::string GetName() const override { return name_; }

private:
    std::string name_;
    std::vector<std::unique_ptr<IEditorCommand>> commands_;
};

//==================================================
// コマンド管理（Undo/Redoスタック）
//==================================================

/// @brief シーンエディターのUndo/Redo管理
class SceneEditorCommands final {
public:
    SceneEditorCommands(Passkey<SceneEditor>, SceneEditorContext *context) : context_(context) {}
    ~SceneEditorCommands() = default;

    /// @brief コマンドを実行してUndoスタックへ積む
    bool Execute(std::unique_ptr<IEditorCommand> command);

    /// @brief 既に適用済みの操作をUndoスタックへ積む（パラメータ編集のコアレス用）
    void PushExecuted(std::unique_ptr<IEditorCommand> command);

    bool Undo();
    bool Redo();

    bool CanUndo() const noexcept { return !GetActiveUndoStack().empty(); }
    bool CanRedo() const noexcept { return !GetActiveRedoStack().empty(); }
    std::string GetUndoName() const { return CanUndo() ? GetActiveUndoStack().back()->GetName() : ""; }
    std::string GetRedoName() const { return CanRedo() ? GetActiveRedoStack().back()->GetName() : ""; }

    /// @brief 履歴を全消去する（シーンロード時など）
    void Clear() {
        LogScope scope;
        undoStack_.clear();
        redoStack_.clear();
        playUndoStack_.clear();
        playRedoStack_.clear();
    }

    //==================================================
    // 再生セッション（ゲームループ再生中のコマンド分離）
    //==================================================
    // 再生中に積まれたコマンドは、停止時のシーン復元によって対象の状態ごと巻き戻されるため
    // 編集時の履歴とは互換性が無い。再生中は専用の一時履歴へ積み、停止時にまとめて破棄する。
    // これにより、編集時の履歴は再生を跨いでもそのまま使用でき、
    // 再生中に編集時のコマンドをUndoしてシーン状態を壊すことも防げる。

    /// @brief 再生セッションを開始する（以後のコマンドは再生用の一時履歴へ積まれる）
    void BeginPlaySession() {
        LogScope scope;
        if (isPlaySession_) return;
        isPlaySession_ = true;
        playUndoStack_.clear();
        playRedoStack_.clear();
    }
    /// @brief 再生セッションを終了し、再生中に積まれたコマンドを破棄する（編集時の履歴へ戻る）
    void EndPlaySession() {
        LogScope scope;
        if (!isPlaySession_) return;
        isPlaySession_ = false;
        playUndoStack_.clear();
        playRedoStack_.clear();
    }
    /// @brief 再生セッション中かどうか
    bool IsPlaySession() const noexcept { return isPlaySession_; }

    /// @brief 履歴表示ImGui（ウィンドウのBegin/Endは呼ばない）
    void ShowHistoryImGui();

private:
    static constexpr size_t kMaxHistory = 128;

    void PushToUndoStack(std::unique_ptr<IEditorCommand> command);

    /// @brief 現在有効なUndo/Redoスタックを取得する（再生セッション中は再生用の一時履歴）
    std::vector<std::unique_ptr<IEditorCommand>> &GetActiveUndoStack() noexcept { return isPlaySession_ ? playUndoStack_ : undoStack_; }
    std::vector<std::unique_ptr<IEditorCommand>> &GetActiveRedoStack() noexcept { return isPlaySession_ ? playRedoStack_ : redoStack_; }
    const std::vector<std::unique_ptr<IEditorCommand>> &GetActiveUndoStack() const noexcept { return isPlaySession_ ? playUndoStack_ : undoStack_; }
    const std::vector<std::unique_ptr<IEditorCommand>> &GetActiveRedoStack() const noexcept { return isPlaySession_ ? playRedoStack_ : redoStack_; }

    SceneEditorContext *context_ = nullptr;
    std::vector<std::unique_ptr<IEditorCommand>> undoStack_;
    std::vector<std::unique_ptr<IEditorCommand>> redoStack_;
    /// @brief 再生セッション中の一時履歴（停止時に破棄される）
    std::vector<std::unique_ptr<IEditorCommand>> playUndoStack_;
    std::vector<std::unique_ptr<IEditorCommand>> playRedoStack_;
    bool isPlaySession_ = false;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
