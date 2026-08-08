#include "SceneEditorCommands.h"
#ifdef USE_IMGUI
#include <imgui.h>

#include "ComponentSerialize/ComponentRegistry.h"
#include "Objects/Components/Transform.h"
#include "Utilities/Translation.h"

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

bool PasteObjectCommand::Execute(SceneEditorContext *context) {
    if (nodes_.empty()) return false;
    EmptyObject *attachParent = attachParentID_.IsValid() ? context->GetSceneObject(attachParentID_) : nullptr;
    EmptyObject *rootObj = nullptr;
    for (size_t i = 0; i < nodes_.size(); ++i) {
        const size_t index = (i == 0) ? insertIndex_ : MAXSIZE_T;
        EmptyObject *obj = context->CreateObjectFromJson(nodes_[i].json, index);
        if (!obj) return false;
        if (i == 0) rootObj = obj;
        // 部分木の根（＝コピー時にparentIndexInSubtreeが-1だったノード）は貼り付け先へ接続する。
        // preserveOriginalRootParent_の場合は各ノードのJSONに残された元の親参照をそのまま使うため何もしない
        // （それ以外の部分木内部の子孫は常にTransformの"parent"参照で自動的に接続済み）。
        if (!preserveOriginalRootParent_ && nodes_[i].parentIndexInSubtree < 0) {
            if (auto *transform = obj->GetComponent<Transform>()) {
                transform->SetParentObject(attachParent);
            }
        }
    }
    return rootObj != nullptr;
}
bool PasteObjectCommand::Undo(SceneEditorContext *context) {
    // 子孫から先に削除する（nodes_はルート→子孫の順で積まれているため逆順に辿る）
    bool allSucceeded = true;
    for (auto it = nodes_.rbegin(); it != nodes_.rend(); ++it) {
        const UUID128 id(it->json.value("objectID", std::string{}));
        auto *obj = context->GetSceneObject(id);
        if (obj && !context->DeleteObject(obj)) allSucceeded = false;
    }
    return allSucceeded;
}

namespace {
/// @brief 指定オブジェクトを根とする部分木のJSONスナップショットをpre-order順で収集する
///        （PasteObjectCommandのクリップボード構築と同じ手法。DeleteObjectCommandの
///        Undo用に、削除で失われる子孫も含めて復元できるようにするため使用する）
void CollectSubtreeSnapshot(SceneEditorContext *context, EmptyObject *obj, int parentIndex, std::vector<PasteObjectCommand::Node> &out) {
    if (!obj || !context) return;
    const int myIndex = static_cast<int>(out.size());
    out.push_back({ context->SaveObjectToJson(obj), parentIndex });
    for (auto *candidate : context->GetSceneObjects()) {
        if (!candidate || candidate == obj) continue;
        auto *candidateTransform = candidate->GetComponent<Transform>();
        if (candidateTransform && candidateTransform->GetParentObject() == obj) {
            CollectSubtreeSnapshot(context, candidate, myIndex, out);
        }
    }
}
} // namespace

bool DeleteObjectCommand::Execute(SceneEditorContext *context) {
    auto *obj = context->GetSceneObject(objectID_);
    if (!obj) return false;
    snapshot_.clear();
    index_ = context->GetObjectIndex(obj);
    CollectSubtreeSnapshot(context, obj, -1, snapshot_);
    // オブジェクト削除は子オブジェクトも連鎖的に削除する（Scene::DeleteObject側で再帰処理される）
    return context->DeleteObject(obj);
}
bool DeleteObjectCommand::Undo(SceneEditorContext *context) {
    if (snapshot_.empty()) return false;
    // pre-order（親→子の順）でそのまま復元すれば、各ノードのTransform "parent" が
    // 元のobjectIDを参照しているため、親が先に生成済みであれば自動的に親子関係も復元される
    EmptyObject *rootObj = nullptr;
    for (size_t i = 0; i < snapshot_.size(); ++i) {
        const size_t index = (i == 0) ? index_ : MAXSIZE_T;
        EmptyObject *obj = context->CreateObjectFromJson(snapshot_[i].json, index);
        if (!obj) return false;
        if (i == 0) rootObj = obj;
    }
    return rootObj != nullptr;
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

bool ObjectStateCommand::Apply(SceneEditorContext *context, const UUID128 &objectID, const JSON &state) {
    auto *obj = context ? context->GetSceneObject(objectID) : nullptr;
    if (!obj) return false;
    obj->SetName(state.value("name", obj->GetName()));
    obj->SetTag(state.value("tag", obj->GetTagName()));
    obj->SetActive(state.value("isActive", obj->IsActive()));
    obj->SetEditorOnly(state.value("editorOnly", obj->IsEditorOnly()));
    return true;
}

bool ObjectStateCommand::Execute(SceneEditorContext *context) {
    return Apply(context, objectID_, after_);
}

bool ObjectStateCommand::Undo(SceneEditorContext *context) {
    return Apply(context, objectID_, before_);
}

//==================================================
// コンポーネント操作コマンド
//==================================================

bool AddComponentCommand::Execute(SceneEditorContext *context) {
    auto *obj = context->GetSceneObject(objectID_);
    if (!obj) return false;
    auto newComponent = CreateObjectComponentByType(componentType_);
    if (!newComponent) return false;
    IObjectComponent *component = obj->AddComponent(std::move(newComponent));
    if (!component) return false;
    componentRef_ = component->GetComponentRef();
    // Redo時は以前の状態を復元する
    if (!state_.empty()) {
        obj->LoadComponentFromJson(component, state_);
    }
    return true;
}
bool AddComponentCommand::Undo(SceneEditorContext *context) {
    auto *obj = context->GetSceneObject(objectID_);
    IObjectComponent *component = context->ResolveComponent(componentRef_);
    if (!obj || !component) return false;
    state_ = obj->SaveComponentToJson(component);
    const bool removed = obj->RemoveComponent(component);
    if (removed) componentRef_ = ComponentRef{};
    return removed;
}

bool RemoveComponentCommand::Execute(SceneEditorContext *context) {
    auto *obj = context->GetSceneObject(objectID_);
    IObjectComponent *component = context->ResolveComponent(componentRef_);
    if (!obj || !component) return false;
    snapshot_ = obj->SaveComponentToJson(component);
    const bool removed = obj->RemoveComponent(component);
    if (removed) componentRef_ = ComponentRef{};
    return removed;
}
bool RemoveComponentCommand::Undo(SceneEditorContext *context) {
    auto *obj = context->GetSceneObject(objectID_);
    if (!obj) return false;
    IObjectComponent *component = obj->AddComponentFromJson(snapshot_);
    componentRef_ = component ? component->GetComponentRef() : ComponentRef{};
    return component != nullptr;
}

bool ComponentEditCommand::Execute(SceneEditorContext *context) {
    auto *obj = context->GetSceneObject(objectID_);
    IObjectComponent *component = context->ResolveComponent(componentRef_);
    if (!obj || !component) return false;
    return obj->LoadComponentFromJson(component, after_);
}
bool ComponentEditCommand::Undo(SceneEditorContext *context) {
    auto *obj = context->GetSceneObject(objectID_);
    IObjectComponent *component = context->ResolveComponent(componentRef_);
    if (!obj || !component) return false;
    return obj->LoadComponentFromJson(component, before_);
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
    auto &undoStack = GetActiveUndoStack();
    if (undoStack.empty() || !context_) return false;
    auto command = std::move(undoStack.back());
    undoStack.pop_back();
    const bool succeeded = command->Undo(context_);
    GetActiveRedoStack().push_back(std::move(command));
    return succeeded;
}

bool SceneEditorCommands::Redo() {
    auto &redoStack = GetActiveRedoStack();
    if (redoStack.empty() || !context_) return false;
    auto command = std::move(redoStack.back());
    redoStack.pop_back();
    const bool succeeded = command->Execute(context_);
    GetActiveUndoStack().push_back(std::move(command));
    return succeeded;
}

void SceneEditorCommands::ShowHistoryImGui() {
    const auto &undoStack = GetActiveUndoStack();
    if (isPlaySession_) {
        ImGui::TextDisabled("%s", TranslationC("editor.history.playing"));
    }
    ImGui::Text("%s%d", TranslationC("editor.history.undostack"), static_cast<int>(undoStack.size()));
    for (auto it = undoStack.rbegin(); it != undoStack.rend(); ++it) {
        ImGui::BulletText("%s", (*it)->GetName().c_str());
    }
}

void SceneEditorCommands::PushToUndoStack(std::unique_ptr<IEditorCommand> command) {
    auto &undoStack = GetActiveUndoStack();
    undoStack.push_back(std::move(command));
    if (undoStack.size() > kMaxHistory) {
        undoStack.erase(undoStack.begin());
    }
    GetActiveRedoStack().clear();
}

} // namespace KashipanEngine

#endif // USE_IMGUI
