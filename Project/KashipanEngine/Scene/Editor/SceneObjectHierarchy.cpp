#include "SceneObjectHierarchy.h"
#include "Scene/Editor/EditorSettings.h"
#include "Scene/Editor/SceneObjectPayload.h"
#include "Scene/Editor/SceneEditorCommands.h"
#include "Objects/Components/Transform.h"

namespace KashipanEngine {

void SceneObjectHierarchy::ShowImGui() {
    RebuildObjectItems();

    ImGui::Begin("Scene Object Hierarchy");

    if (EditorSettings::PersistentCollapsingHeader("Objects", "hierarchy.objects")) {
        size_t index = 0;
        for (size_t i = 0; i < objectItems_.size(); ++i) {
            ShowObjectItem(objectItems_[i], index);
            ++index;
        }
    }

    ShowHierarchyContextMenu();
    ApplyDragAndDrop();

    ImGui::End();
}

void SceneObjectHierarchy::RebuildObjectItems() {
    const auto &objects = editorContext_->GetSceneObjects();
    objectItems_.clear();
    objectParentMap_.clear();
    objectItems_.reserve(objects.size());
    objectParentMap_.reserve(objects.size());

    for (size_t i = 0; i < objects.size(); ++i) {
        const auto &obj = objects[i];
        if (!obj) continue;

        objectParentMap_.emplace(obj.get(), std::vector<std::pair<EmptyObject *, size_t>>{});
        auto *transform = obj->GetComponent<Transform>();
        auto *parent = transform ? transform->GetParentObject() : nullptr;
        if (parent) {
            objectParentMap_[parent].push_back({ obj.get(), i });
        } else {
            ObjectItem item{};
            item.object = obj.get();
            item.name = obj->GetName();
            item.depth = 0;
            item.originalIndex = i;
            objectItems_.push_back(std::move(item));
        }
    }

    for (auto &item : objectItems_) {
        RecursivelyBuildObjectItems(item.object, item, 0);
    }
}

void SceneObjectHierarchy::RecursivelyBuildObjectItems(EmptyObject *obj, ObjectItem &item, size_t depth) {
    for (const auto &childPair : objectParentMap_[obj]) {
        ObjectItem childItem{};
        childItem.object = childPair.first;
        childItem.name = childPair.first->GetName();
        childItem.depth = depth + 1;
        childItem.originalIndex = childPair.second;
        item.children.push_back(std::move(childItem));
        RecursivelyBuildObjectItems(childPair.first, item.children.back(), depth + 1);
    }
}

void SceneObjectHierarchy::ShowObjectItem(const ObjectItem &item, size_t &index) {
    // インデックスではなくオブジェクトをIDにすることで、
    // オブジェクトの追加/削除があっても開閉状態が別のオブジェクトへずれないようにする
    ImGui::PushID(item.object);
    for (size_t i = 0; i < item.depth; ++i) {
        ImGui::Indent();
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
    if (item.children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    } else {
        flags |= ImGuiTreeNodeFlags_OpenOnArrow;
    }
    if (selectedObjectIndex_ == index) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    // 開閉状態を保存・復元する（デフォルトは開いた状態）
    bool storedOpen = true;
    std::string settingsKey;
    if (!item.children.empty()) {
        settingsKey = "hierarchy.object." + item.object->GetObjectID().ToString();
        storedOpen = EditorSettings::GetBool(settingsKey, true);
        ImGui::SetNextItemOpen(storedOpen, ImGuiCond_Once);
    }

    const bool isOpen = ImGui::TreeNodeEx(item.name.c_str(), flags);
    if (!item.children.empty() && isOpen != storedOpen) {
        EditorSettings::SetBool(settingsKey, isOpen);
    }
    DragAndDropObject(const_cast<ObjectItem *>(&item));
    ShowObjectContextMenu(item.object);

    if (ImGui::IsItemClicked()) {
        selectedObjectIndex_ = index;
        selectedObject_ = item.object;
    }

    if (!item.children.empty() && isOpen) {
        for (const auto &child : item.children) {
            ShowObjectItem(child, ++index);
        }
        ImGui::TreePop();
    }

    for (size_t i = 0; i < item.depth; ++i) {
        ImGui::Unindent();
    }
    ImGui::PopID();
}

void SceneObjectHierarchy::ShowObjectContextMenu(EmptyObject *obj) {
    if (ImGui::BeginPopupContextItem("ObjectContextMenu")) {
        if (ImGui::MenuItem("Delete Object")) {
            if (commands_) {
                commands_->Execute(std::make_unique<DeleteObjectCommand>(obj));
            } else {
                editorContext_->DeleteObject(obj);
            }
            ClearSelection();
        }
        ImGui::EndPopup();
    }
}

void SceneObjectHierarchy::ShowHierarchyContextMenu() {
    if (ImGui::BeginPopupContextWindow("HierarchyContextMenu")) {
        if (ImGui::MenuItem("Create Empty Object")) {
            if (commands_) {
                commands_->Execute(std::make_unique<CreateObjectCommand>("EmptyObject"));
            } else {
                editorContext_->CreateEmptyObject("EmptyObject");
            }
        }
        ImGui::EndPopup();
    }
}

void SceneObjectHierarchy::ShowAddObjectMenu(EmptyObject *parent) {
    auto *obj = editorContext_->CreateEmptyObject("EmptyObject");
    if (obj && parent) {
        auto *transform = obj->GetComponent<Transform>();
        if (!transform) {
            transform = obj->AddComponent<Transform>();
        }
        if (transform) {
            transform->SetParentObject(parent);
        }
    }
}

void SceneObjectHierarchy::DragAndDropObject(ObjectItem *objItem) {
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        // 外部（インスペクター等）でも受け取れる共有ペイロード型で送る
        SceneObjectDragDropPayload dndPayload;
        dndPayload.object = objItem->object;
        dndPayload.internalItem = objItem;
        ImGui::SetDragDropPayload(kSceneObjectDragDropType, &dndPayload, sizeof(dndPayload));
        ImGui::Text("%s", objItem->name.c_str());
        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
        ImGuiDragDropFlags targetFlags = ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(kSceneObjectDragDropType, targetFlags)) {
            DropPosition dropPosition = DragAndDropTargetCommon();
            if (payload->IsDelivery()) {
                IM_ASSERT(payload->DataSize == sizeof(SceneObjectDragDropPayload));
                auto *dndPayload = static_cast<const SceneObjectDragDropPayload *>(payload->Data);
                dragDropPayload_.objectItemSource = static_cast<ObjectItem *>(dndPayload->internalItem);
                dragDropPayload_.objectItemTarget = objItem;
                dragDropPayload_.position = dropPosition;
            }
        }
        ImGui::EndDragDropTarget();
    }
}

void SceneObjectHierarchy::ApplyDragAndDrop() {
    if (!dragDropPayload_.objectItemSource || !dragDropPayload_.objectItemTarget) return;

    ObjectItem *sourceItem = dragDropPayload_.objectItemSource;
    ObjectItem *targetItem = dragDropPayload_.objectItemTarget;
    DropPosition position = dragDropPayload_.position;

    size_t moveIndex = targetItem->originalIndex;
    auto *targetTransform = targetItem->object->GetComponent<Transform>();
    auto *targetParent = targetTransform ? targetTransform->GetParentObject() : nullptr;
    auto *sourceTransform = sourceItem->object->GetComponent<Transform>();
    if (!sourceTransform) {
        sourceTransform = sourceItem->object->AddComponent<Transform>();
    }

    bool isParentSet = false;
    if (sourceTransform) {
        // Undo用に移動前の状態を保存しておく
        const JSON transformBefore = sourceItem->object->SaveComponentToJson(sourceTransform);
        const size_t indexBefore = editorContext_->GetObjectIndex(sourceItem->object);
        size_t indexAfter = MAXSIZE_T;

        switch (position) {
        case DropPosition::Above:
            isParentSet = sourceTransform->SetParentObject(targetParent);
            if (isParentSet) {
                indexAfter = moveIndex;
                editorContext_->MoveObject(sourceItem->object, indexAfter);
            }
            break;
        case DropPosition::Below:
            isParentSet = sourceTransform->SetParentObject(targetParent);
            if (isParentSet) {
                indexAfter = moveIndex + 1;
                editorContext_->MoveObject(sourceItem->object, indexAfter);
            }
            break;
        case DropPosition::Inside:
            isParentSet = sourceTransform->SetParentObject(targetItem->object);
            if (isParentSet) {
                indexAfter = moveIndex;
                editorContext_->MoveObject(sourceItem->object, indexAfter);
            }
            break;
        default:
            break;
        }

        // 親変更と並び替えをひとつの操作としてUndo履歴へ積む
        if (isParentSet && commands_) {
            const JSON transformAfter = sourceItem->object->SaveComponentToJson(sourceTransform);
            auto composite = std::make_unique<CompositeCommand>("Move Object: " + sourceItem->object->GetName());
            if (transformBefore != transformAfter) {
                composite->AddCommand(std::make_unique<ComponentEditCommand>(
                    sourceItem->object, sourceTransform, transformBefore, transformAfter));
            }
            composite->AddCommand(std::make_unique<MoveObjectCommand>(sourceItem->object, indexBefore, indexAfter));
            commands_->PushExecuted(std::move(composite));
        }
    }

    dragDropPayload_.objectItemSource = nullptr;
    dragDropPayload_.objectItemTarget = nullptr;
}

SceneObjectHierarchy::DropPosition SceneObjectHierarchy::DragAndDropTargetCommon() {
    ImVec2 itemRectMin = ImGui::GetItemRectMin();
    ImVec2 itemRectMax = ImGui::GetItemRectMax();
    float mouseY = ImGui::GetMousePos().y;
    float itemHeight = itemRectMax.y - itemRectMin.y;

    const float thresholdUpper = itemRectMin.y + itemHeight * 0.25f;
    const float thresholdLower = itemRectMax.y - itemHeight * 0.25f;
    DropPosition dropPosition;

    if (mouseY < thresholdUpper) {
        dropPosition = DropPosition::Above;
    } else if (mouseY > thresholdLower) {
        dropPosition = DropPosition::Below;
    } else {
        dropPosition = DropPosition::Inside;
    }

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    ImU32 highlightColor = IM_COL32(0, 255, 0, 255);
    float lineThickness = 2.0f;

    if (dropPosition == DropPosition::Above) {
        drawList->AddLine(ImVec2(itemRectMin.x, itemRectMin.y), ImVec2(itemRectMax.x, itemRectMin.y), highlightColor, lineThickness);
    } else if (dropPosition == DropPosition::Below) {
        drawList->AddLine(ImVec2(itemRectMin.x, itemRectMax.y), ImVec2(itemRectMax.x, itemRectMax.y), highlightColor, lineThickness);
    } else {
        drawList->AddRect(itemRectMin, itemRectMax, highlightColor, 0.0f, 0, lineThickness);
    }

    return dropPosition;
}

} // namespace KashipanEngine
