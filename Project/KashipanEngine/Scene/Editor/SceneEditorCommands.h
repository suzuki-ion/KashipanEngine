#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include <memory>
#include <string>
#include <vector>

#include "Scene/SceneEditorContext.h"
#include "ComponentSerialize/ComponentRegistry.h"

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

    bool Execute(SceneEditorContext *context) override {
        return context->CreateEmptyObject(name_, objectID_, index_) != nullptr;
    }
    bool Undo(SceneEditorContext *context) override {
        auto *obj = context->GetSceneObject(objectID_);
        return obj && context->DeleteObject(obj);
    }
    std::string GetName() const override { return "Create Object: " + name_; }

    const UUID128 &GetObjectID() const noexcept { return objectID_; }

private:
    std::string name_;
    UUID128 objectID_;
    size_t index_ = MAXSIZE_T;
};

/// @brief オブジェクト削除コマンド
class DeleteObjectCommand final : public IEditorCommand {
public:
    explicit DeleteObjectCommand(EmptyObject *obj)
        : objectID_(obj ? obj->GetObjectID() : UUID128()), name_(obj ? obj->GetName() : "") {}

    bool Execute(SceneEditorContext *context) override {
        auto *obj = context->GetSceneObject(objectID_);
        if (!obj) return false;
        snapshot_ = context->SaveObjectToJson(obj);
        index_ = context->GetObjectIndex(obj);
        return context->DeleteObject(obj);
    }
    bool Undo(SceneEditorContext *context) override {
        return context->CreateObjectFromJson(snapshot_, index_) != nullptr;
    }
    std::string GetName() const override { return "Delete Object: " + name_; }

private:
    UUID128 objectID_;
    std::string name_;
    JSON snapshot_;
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

    bool Execute(SceneEditorContext *context) override {
        auto *obj = context->GetSceneObject(objectID_);
        if (!obj) return false;
        if (oldIndex_ == MAXSIZE_T) {
            oldIndex_ = context->GetObjectIndex(obj);
        }
        return context->MoveObject(obj, newIndex_);
    }
    bool Undo(SceneEditorContext *context) override {
        auto *obj = context->GetSceneObject(objectID_);
        if (!obj) return false;
        return context->MoveObject(obj, oldIndex_);
    }
    std::string GetName() const override { return "Move Object"; }

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

    bool Execute(SceneEditorContext *context) override {
        auto *obj = context->GetSceneObject(objectID_);
        if (!obj) return false;
        obj->SetName(newName_);
        obj->SetActive(newActive_);
        return true;
    }
    bool Undo(SceneEditorContext *context) override {
        auto *obj = context->GetSceneObject(objectID_);
        if (!obj) return false;
        obj->SetName(oldName_);
        obj->SetActive(oldActive_);
        return true;
    }
    std::string GetName() const override { return "Edit Object: " + newName_; }

private:
    UUID128 objectID_;
    std::string oldName_;
    std::string newName_;
    bool oldActive_ = true;
    bool newActive_ = true;
};

//==================================================
// コンポーネント操作コマンド
//==================================================

/// @brief コンポーネント追加コマンド
class AddComponentCommand final : public IEditorCommand {
public:
    AddComponentCommand(EmptyObject *obj, const std::string &componentType)
        : objectID_(obj ? obj->GetObjectID() : UUID128()), componentType_(componentType) {}

    bool Execute(SceneEditorContext *context) override {
        auto *obj = context->GetSceneObject(objectID_);
        if (!obj) return false;
        auto newComponent = CreateObjectComponentByType(componentType_);
        if (!newComponent) return false;
        component_ = obj->AddComponent(std::move(newComponent));
        if (!component_) return false;
        // Redo時は以前の状態を復元する
        if (!state_.empty()) {
            obj->LoadComponentFromJson(component_, state_);
        }
        return true;
    }
    bool Undo(SceneEditorContext *context) override {
        auto *obj = context->GetSceneObject(objectID_);
        if (!obj || !component_) return false;
        state_ = obj->SaveComponentToJson(component_);
        const bool removed = obj->RemoveComponent(component_);
        if (removed) component_ = nullptr;
        return removed;
    }
    std::string GetName() const override { return "Add Component: " + componentType_; }

private:
    UUID128 objectID_;
    std::string componentType_;
    IObjectComponent *component_ = nullptr;
    JSON state_;
};

/// @brief コンポーネント削除コマンド
class RemoveComponentCommand final : public IEditorCommand {
public:
    RemoveComponentCommand(EmptyObject *obj, IObjectComponent *component)
        : objectID_(obj ? obj->GetObjectID() : UUID128()), component_(component),
          componentType_(component ? component->GetComponentType() : "") {}

    bool Execute(SceneEditorContext *context) override {
        auto *obj = context->GetSceneObject(objectID_);
        if (!obj || !component_) return false;
        snapshot_ = obj->SaveComponentToJson(component_);
        const bool removed = obj->RemoveComponent(component_);
        if (removed) component_ = nullptr;
        return removed;
    }
    bool Undo(SceneEditorContext *context) override {
        auto *obj = context->GetSceneObject(objectID_);
        if (!obj) return false;
        component_ = obj->AddComponentFromJson(snapshot_);
        return component_ != nullptr;
    }
    std::string GetName() const override { return "Remove Component: " + componentType_; }

private:
    UUID128 objectID_;
    IObjectComponent *component_ = nullptr;
    std::string componentType_;
    JSON snapshot_;
};

/// @brief コンポーネントのパラメータ変更コマンド（変更前後のJSONスナップショット）
/// @details 変更適用済みの状態で積まれる想定（PushExecutedを使用する）
class ComponentEditCommand final : public IEditorCommand {
public:
    ComponentEditCommand(EmptyObject *obj, IObjectComponent *component, JSON before, JSON after)
        : objectID_(obj ? obj->GetObjectID() : UUID128()), component_(component),
          componentType_(component ? component->GetComponentType() : ""),
          before_(std::move(before)), after_(std::move(after)) {}

    bool Execute(SceneEditorContext *context) override {
        auto *obj = context->GetSceneObject(objectID_);
        if (!obj || !component_) return false;
        return obj->LoadComponentFromJson(component_, after_);
    }
    bool Undo(SceneEditorContext *context) override {
        auto *obj = context->GetSceneObject(objectID_);
        if (!obj || !component_) return false;
        return obj->LoadComponentFromJson(component_, before_);
    }
    std::string GetName() const override { return "Edit Component: " + componentType_; }

private:
    UUID128 objectID_;
    IObjectComponent *component_ = nullptr;
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
        if (command) commands_.push_back(std::move(command));
    }
    bool IsEmpty() const noexcept { return commands_.empty(); }

    bool Execute(SceneEditorContext *context) override {
        bool allSucceeded = true;
        for (auto &command : commands_) {
            if (!command->Execute(context)) allSucceeded = false;
        }
        return allSucceeded;
    }
    bool Undo(SceneEditorContext *context) override {
        bool allSucceeded = true;
        for (auto it = commands_.rbegin(); it != commands_.rend(); ++it) {
            if (!(*it)->Undo(context)) allSucceeded = false;
        }
        return allSucceeded;
    }
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
    bool Execute(std::unique_ptr<IEditorCommand> command) {
        if (!command || !context_) return false;
        if (!command->Execute(context_)) return false;
        PushToUndoStack(std::move(command));
        return true;
    }

    /// @brief 既に適用済みの操作をUndoスタックへ積む（パラメータ編集のコアレス用）
    void PushExecuted(std::unique_ptr<IEditorCommand> command) {
        if (!command) return;
        PushToUndoStack(std::move(command));
    }

    bool Undo() {
        if (undoStack_.empty() || !context_) return false;
        auto command = std::move(undoStack_.back());
        undoStack_.pop_back();
        const bool succeeded = command->Undo(context_);
        redoStack_.push_back(std::move(command));
        return succeeded;
    }

    bool Redo() {
        if (redoStack_.empty() || !context_) return false;
        auto command = std::move(redoStack_.back());
        redoStack_.pop_back();
        const bool succeeded = command->Execute(context_);
        undoStack_.push_back(std::move(command));
        return succeeded;
    }

    bool CanUndo() const noexcept { return !undoStack_.empty(); }
    bool CanRedo() const noexcept { return !redoStack_.empty(); }
    std::string GetUndoName() const { return CanUndo() ? undoStack_.back()->GetName() : ""; }
    std::string GetRedoName() const { return CanRedo() ? redoStack_.back()->GetName() : ""; }

    /// @brief 履歴を全消去する（シーンロード時など）
    void Clear() {
        undoStack_.clear();
        redoStack_.clear();
    }

    /// @brief 履歴表示ImGui（ウィンドウのBegin/Endは呼ばない）
    void ShowHistoryImGui() {
        ImGui::Text("Undo Stack: %d", static_cast<int>(undoStack_.size()));
        for (auto it = undoStack_.rbegin(); it != undoStack_.rend(); ++it) {
            ImGui::BulletText("%s", (*it)->GetName().c_str());
        }
    }

private:
    static constexpr size_t kMaxHistory = 128;

    void PushToUndoStack(std::unique_ptr<IEditorCommand> command) {
        undoStack_.push_back(std::move(command));
        if (undoStack_.size() > kMaxHistory) {
            undoStack_.erase(undoStack_.begin());
        }
        redoStack_.clear();
    }

    SceneEditorContext *context_ = nullptr;
    std::vector<std::unique_ptr<IEditorCommand>> undoStack_;
    std::vector<std::unique_ptr<IEditorCommand>> redoStack_;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
