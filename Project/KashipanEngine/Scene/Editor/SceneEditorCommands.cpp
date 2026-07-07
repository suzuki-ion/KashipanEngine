#include "SceneEditorCommands.h"
#ifdef USE_IMGUI
#include <imgui.h>

#include "ComponentSerialize/ComponentRegistry.h"
#include "Objects/Components/Transform.h"

namespace KashipanEngine {

//==================================================
// オブジェクト操作コマンド
//==================================================

bool CreateObjectCommand::Execute(SceneEditorContext *context) {
    return context->CreateEmptyObject(name_, objectID_, index_) != nullptr;
}
bool CreateObjectCommand::Undo(SceneEditorContext *context) {
    auto *obj = context->GetSceneObject(objectID_);
    return obj && context->DeleteObject(obj);
}

bool CreateSiblingObjectCommand::Execute(SceneEditorContext *context) {
    auto *obj = context->CreateEmptyObject(name_, objectID_, index_);
    if (!obj) return false;
    // 参照オブジェクトと同じ親を設定する（参照オブジェクトがルートの場合は親無しのまま）
    auto *referenceObject = context->GetSceneObject(referenceObjectID_);
    auto *referenceTransform = referenceObject ? referenceObject->GetComponent<Transform>() : nullptr;
    if (auto *parent = referenceTransform ? referenceTransform->GetParentObject() : nullptr) {
        if (auto *transform = obj->GetComponent<Transform>()) {
            transform->SetParentObject(parent);
        }
    }
    return true;
}
bool CreateSiblingObjectCommand::Undo(SceneEditorContext *context) {
    auto *obj = context->GetSceneObject(objectID_);
    return obj && context->DeleteObject(obj);
}

bool CreateChildObjectCommand::Execute(SceneEditorContext *context) {
    // 末尾（配列の最後）に作成すると、既存の子はそれより前に作られているため
    // 兄弟内で最後の子として並ぶ
    auto *obj = context->CreateEmptyObject(name_, objectID_, MAXSIZE_T);
    if (!obj) return false;
    if (auto *parent = context->GetSceneObject(parentObjectID_)) {
        if (auto *transform = obj->GetComponent<Transform>()) {
            transform->SetParentObject(parent);
        }
    }
    return true;
}
bool CreateChildObjectCommand::Undo(SceneEditorContext *context) {
    auto *obj = context->GetSceneObject(objectID_);
    return obj && context->DeleteObject(obj);
}

bool DeleteObjectCommand::Execute(SceneEditorContext *context) {
    auto *obj = context->GetSceneObject(objectID_);
    if (!obj) return false;
    snapshot_ = context->SaveObjectToJson(obj);
    index_ = context->GetObjectIndex(obj);
    return context->DeleteObject(obj);
}
bool DeleteObjectCommand::Undo(SceneEditorContext *context) {
    return context->CreateObjectFromJson(snapshot_, index_) != nullptr;
}

bool MoveObjectCommand::Execute(SceneEditorContext *context) {
    auto *obj = context->GetSceneObject(objectID_);
    if (!obj) return false;
    if (oldIndex_ == MAXSIZE_T) {
        oldIndex_ = context->GetObjectIndex(obj);
    }
    return context->MoveObject(obj, newIndex_);
}
bool MoveObjectCommand::Undo(SceneEditorContext *context) {
    auto *obj = context->GetSceneObject(objectID_);
    if (!obj) return false;
    return context->MoveObject(obj, oldIndex_);
}

bool ObjectPropertyCommand::Execute(SceneEditorContext *context) {
    auto *obj = context->GetSceneObject(objectID_);
    if (!obj) return false;
    obj->SetName(newName_);
    obj->SetActive(newActive_);
    return true;
}
bool ObjectPropertyCommand::Undo(SceneEditorContext *context) {
    auto *obj = context->GetSceneObject(objectID_);
    if (!obj) return false;
    obj->SetName(oldName_);
    obj->SetActive(oldActive_);
    return true;
}

//==================================================
// コンポーネント操作コマンド
//==================================================

bool AddComponentCommand::Execute(SceneEditorContext *context) {
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
bool AddComponentCommand::Undo(SceneEditorContext *context) {
    auto *obj = context->GetSceneObject(objectID_);
    if (!obj || !component_) return false;
    state_ = obj->SaveComponentToJson(component_);
    const bool removed = obj->RemoveComponent(component_);
    if (removed) component_ = nullptr;
    return removed;
}

bool RemoveComponentCommand::Execute(SceneEditorContext *context) {
    auto *obj = context->GetSceneObject(objectID_);
    if (!obj || !component_) return false;
    snapshot_ = obj->SaveComponentToJson(component_);
    const bool removed = obj->RemoveComponent(component_);
    if (removed) component_ = nullptr;
    return removed;
}
bool RemoveComponentCommand::Undo(SceneEditorContext *context) {
    auto *obj = context->GetSceneObject(objectID_);
    if (!obj) return false;
    component_ = obj->AddComponentFromJson(snapshot_);
    return component_ != nullptr;
}

bool ComponentEditCommand::Execute(SceneEditorContext *context) {
    auto *obj = context->GetSceneObject(objectID_);
    if (!obj || !component_) return false;
    return obj->LoadComponentFromJson(component_, after_);
}
bool ComponentEditCommand::Undo(SceneEditorContext *context) {
    auto *obj = context->GetSceneObject(objectID_);
    if (!obj || !component_) return false;
    return obj->LoadComponentFromJson(component_, before_);
}

//==================================================
// 複合コマンド
//==================================================

bool CompositeCommand::Execute(SceneEditorContext *context) {
    bool allSucceeded = true;
    for (auto &command : commands_) {
        if (!command->Execute(context)) allSucceeded = false;
    }
    return allSucceeded;
}
bool CompositeCommand::Undo(SceneEditorContext *context) {
    bool allSucceeded = true;
    for (auto it = commands_.rbegin(); it != commands_.rend(); ++it) {
        if (!(*it)->Undo(context)) allSucceeded = false;
    }
    return allSucceeded;
}

//==================================================
// コマンド管理（Undo/Redoスタック）
//==================================================

bool SceneEditorCommands::Execute(std::unique_ptr<IEditorCommand> command) {
    if (!command || !context_) return false;
    if (!command->Execute(context_)) return false;
    PushToUndoStack(std::move(command));
    return true;
}

void SceneEditorCommands::PushExecuted(std::unique_ptr<IEditorCommand> command) {
    if (!command) return;
    PushToUndoStack(std::move(command));
}

bool SceneEditorCommands::Undo() {
    if (undoStack_.empty() || !context_) return false;
    auto command = std::move(undoStack_.back());
    undoStack_.pop_back();
    const bool succeeded = command->Undo(context_);
    redoStack_.push_back(std::move(command));
    return succeeded;
}

bool SceneEditorCommands::Redo() {
    if (redoStack_.empty() || !context_) return false;
    auto command = std::move(redoStack_.back());
    redoStack_.pop_back();
    const bool succeeded = command->Execute(context_);
    undoStack_.push_back(std::move(command));
    return succeeded;
}

void SceneEditorCommands::ShowHistoryImGui() {
    ImGui::Text("Undo Stack: %d", static_cast<int>(undoStack_.size()));
    for (auto it = undoStack_.rbegin(); it != undoStack_.rend(); ++it) {
        ImGui::BulletText("%s", (*it)->GetName().c_str());
    }
}

void SceneEditorCommands::PushToUndoStack(std::unique_ptr<IEditorCommand> command) {
    undoStack_.push_back(std::move(command));
    if (undoStack_.size() > kMaxHistory) {
        undoStack_.erase(undoStack_.begin());
    }
    redoStack_.clear();
}

} // namespace KashipanEngine

#endif // USE_IMGUI
