#include "SceneObjectHierarchy.h"
#include "Objects/Components/2D/Transform2D.h"
#include "Objects/Components/3D/Transform3D.h"

namespace KashipanEngine {

void SceneObjectHierarchy::ShowImGui() {
    RebuildObject2DItems();
    RebuildObjectItems();

    ImGui::Begin("Scene Object Hierarchy");

    // 2Dオブジェクトの表示
    if (ImGui::CollapsingHeader("2D Objects", ImGuiTreeNodeFlags_DefaultOpen)) {
        size_t index = 0;
        for (size_t i = 0; i < object2DItems_.size(); ++i) {
            ShowObject2DItem(object2DItems_[i], index);
            ++index;
        }
    }

    // 3Dオブジェクトの表示
    if (ImGui::CollapsingHeader("3D Objects", ImGuiTreeNodeFlags_DefaultOpen)) {
        size_t index = 0;
        for (size_t i = 0; i < object3DItems_.size(); ++i) {
            ShowObjectItem(object3DItems_[i], index);
            ++index;
        }
    }

    // ドラッグアンドドロップの適用
    ApplyDragAndDrop2D();
    ApplyDragAndDrop3D();

    ImGui::End();
}

void SceneObjectHierarchy::RebuildObject2DItems() {
    const auto &objects2D = editorContext_->GetObjects2D();
    object2DItems_.clear();
    object2DParentMap_.clear();
    object2DItems_.reserve(objects2D.size());
    object2DParentMap_.reserve(objects2D.size());

    // 親から子を辿れるように、親子関係を構築する
    for (size_t i = 0; i < objects2D.size(); ++i) {
        const auto &obj = objects2D[i];
        if (!obj) continue;
        object2DParentMap_.emplace(obj.get(), std::vector<std::pair<Object2DBase *, size_t>>{});
        auto *transform = obj->GetComponent2D<Transform2D>();
        auto *parent = transform ? transform->GetParentObject() : nullptr;
        if (parent) {
            object2DParentMap_[parent].push_back({obj.get(), i});
        } else {
            // 親がいない場合はトップレベルのアイテムとして追加
            Object2DItem item{};
            item.object = obj.get();
            item.name = obj->GetName();
            item.depth = 0;
            item.originalIndex = i;
            object2DItems_.push_back(std::move(item));
        }
    }

    // 子アイテムを再帰的に追加する
    for (auto &item : object2DItems_) {
        RecursivelyBuildObject2DItems(item.object, item, 0);
    }
}

void SceneObjectHierarchy::RebuildObjectItems() {
    const auto &objects3D = editorContext_->GetObjects3D();
    object3DItems_.clear();
    object3DParentMap_.clear();
    object3DItems_.reserve(objects3D.size());
    object3DParentMap_.reserve(objects3D.size());

    // 親から子を辿れるように、親子関係を構築する
    for (size_t i = 0; i < objects3D.size(); ++i) {
        const auto &obj = objects3D[i];
        if (!obj) continue;
        object3DParentMap_.emplace(obj.get(), std::vector<std::pair<EmptyObject *, size_t>>{});
        auto *transform = obj->GetComponent3D<Transform3D>();
        auto *parent = transform ? transform->GetParentObject() : nullptr;
        if (parent) {
            object3DParentMap_[parent].push_back({obj.get(), i});
        } else {
            // 親がいない場合はトップレベルのアイテムとして追加
            ObjectItem item{};
            item.object = obj.get();
            item.name = obj->GetName();
            item.depth = 0;
            item.originalIndex = i;
            object3DItems_.push_back(std::move(item));
        }
    }

    // 子アイテムを再帰的に追加する
    for (auto &item : object3DItems_) {
        RecursivelyBuildObjectItems(item.object, item, 0);
    }
}

void SceneObjectHierarchy::RecursivelyBuildObject2DItems(Object2DBase *obj, Object2DItem &item, size_t depth) {
    for (const auto &childPair : object2DParentMap_[obj]) {
        Object2DItem childItem{};
        childItem.object = childPair.first;
        childItem.name = childPair.first->GetName();
        childItem.depth = depth + 1;
        childItem.originalIndex = childPair.second;
        item.children.push_back(std::move(childItem));
        RecursivelyBuildObject2DItems(childPair.first, item.children.back(), depth + 1);
    }
}

void SceneObjectHierarchy::RecursivelyBuildObjectItems(EmptyObject *obj, ObjectItem &item, size_t depth) {
    for (const auto &childPair : object3DParentMap_[obj]) {
        ObjectItem childItem{};
        childItem.object = childPair.first;
        childItem.name = childPair.first->GetName();
        childItem.depth = depth + 1;
        childItem.originalIndex = childPair.second;
        item.children.push_back(std::move(childItem));
        RecursivelyBuildObjectItems(childPair.first, item.children.back(), depth + 1);
    }
}

void SceneObjectHierarchy::ShowObject2DItem(const Object2DItem &item, size_t &index) {
    ImGui::PushID(static_cast<int>(index));
    // 深さの分だけインデントを追加
    for (size_t i = 0; i < item.depth; ++i) {
        ImGui::Indent();
    }
    // 子アイテムがある場合はツリーノードとして表示
    if (!item.children.empty()) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (selectedObjectType_ == SelectedObjectType::Object2D && selectedObject2DIndex_ == index) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        bool isOpen = ImGui::TreeNodeEx(item.name.c_str(), flags);
        DragAndDropObject2D(const_cast<Object2DItem *>(&item));
        if (ImGui::IsItemClicked()) {
            selectedObjectType_ = SelectedObjectType::Object2D;
            selectedObject2DIndex_ = index;
            selectedObjectIndex_ = SIZE_MAX;
            selectedObject2D_ = item.object;
        }
        if (isOpen) {
            for (const auto &child : item.children) {
                ShowObject2DItem(child, ++index);
            }
            ImGui::TreePop();
        }
    } else {
        // 子アイテムがない場合は通常のテキストとして表示
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (selectedObjectType_ == SelectedObjectType::Object2D && selectedObject2DIndex_ == index) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        ImGui::TreeNodeEx(item.name.c_str(), flags);
        DragAndDropObject2D(const_cast<Object2DItem *>(&item));
        if (ImGui::IsItemClicked()) {
            selectedObjectType_ = SelectedObjectType::Object2D;
            selectedObject2DIndex_ = index;
            selectedObjectIndex_ = SIZE_MAX;
            selectedObject2D_ = item.object;
        }
    }
    // インデントを戻す
    for (size_t i = 0; i < item.depth; ++i) {
        ImGui::Unindent();
    }
    ImGui::PopID();
}

void SceneObjectHierarchy::ShowObjectItem(const ObjectItem &item, size_t &index) {
    ImGui::PushID(static_cast<int>(index));
    // 深さの分だけインデントを追加
    for (size_t i = 0; i < item.depth; ++i) {
        ImGui::Indent();
    }
    // 子アイテムがある場合はツリーノードとして表示
    if (!item.children.empty()) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (selectedObjectType_ == SelectedObjectType::Object && selectedObjectIndex_ == index) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        bool isOpen = ImGui::TreeNodeEx(item.name.c_str(), flags);
        DragAndDropObject(const_cast<ObjectItem *>(&item));
        if (ImGui::IsItemClicked()) {
            selectedObjectType_ = SelectedObjectType::Object;
            selectedObjectIndex_ = index;
            selectedObject2DIndex_ = SIZE_MAX;
            selectedObject_ = item.object;
        }
        if (isOpen) {
            for (const auto &child : item.children) {
                ShowObjectItem(child, ++index);
            }
            ImGui::TreePop();
        }
    } else {
        // 子アイテムがない場合は通常のテキストとして表示
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (selectedObjectType_ == SelectedObjectType::Object && selectedObjectIndex_ == index) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        ImGui::TreeNodeEx(item.name.c_str(), flags);
        DragAndDropObject(const_cast<ObjectItem *>(&item));
        if (ImGui::IsItemClicked()) {
            selectedObjectType_ = SelectedObjectType::Object;
            selectedObjectIndex_ = index;
            selectedObject2DIndex_ = SIZE_MAX;
            selectedObject_ = item.object;
        }
    }
    // インデントを戻す
    for (size_t i = 0; i < item.depth; ++i) {
        ImGui::Unindent();
    }
    ImGui::PopID();
}

void SceneObjectHierarchy::ShowObject2DContextMenu(Object2DBase *obj) {
    if (ImGui::BeginPopupContextItem("Object2DContextMenu")) {
        if (ImGui::MenuItem("Delete Object")) {
            editorContext_->RemoveObject2D(obj);
            selectedObjectType_ = SelectedObjectType::None;
            selectedObject2DIndex_ = SIZE_MAX;
            selectedObject2D_ = nullptr;
        }
        ImGui::EndPopup();
    }
}

void SceneObjectHierarchy::ShowObjectContextMenu(EmptyObject *obj) {
    if (ImGui::BeginPopupContextItem("ObjectContextMenu")) {
        if (ImGui::MenuItem("Delete Object")) {
            editorContext_->RemoveObject(obj);
            selectedObjectType_ = SelectedObjectType::None;
            selectedObjectIndex_ = SIZE_MAX;
            selectedObject_ = nullptr;
        }
        ImGui::EndPopup();
    }
}

void SceneObjectHierarchy::ShowHierarchyContextMenu() {
    if (ImGui::BeginPopupContextWindow("HierarchyContextMenu")) {
        ImGui::EndPopup();
    }
}

void SceneObjectHierarchy::ShowAddObject2DMenu(Object2DBase *parent) {
    (void)parent;
}

void SceneObjectHierarchy::ShowAddObjectMenu(EmptyObject *parent) {
    (void)parent;
}

void SceneObjectHierarchy::DragAndDropObject2D(Object2DItem *objItem) {
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        ImGui::SetDragDropPayload("DND_OBJECT2D", &objItem, sizeof(Object2DItem *));
        ImGui::Text("%s", objItem->name.c_str());
        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
        ImGuiDragDropFlags targetFlags = ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("DND_OBJECT2D", targetFlags)) {
            DropPosition dropPosition = DragAndDropTargetCommon();
            // ドロップ処理
            if (payload->IsDelivery()) {
                IM_ASSERT(payload->DataSize == sizeof(Object2DItem *));
                dragDropPayload2D_.objectItemSource = *(Object2DItem **)payload->Data;
                dragDropPayload2D_.objectItemTarget = objItem;
                dragDropPayload2D_.position = dropPosition;
            }
        }
        ImGui::EndDragDropTarget();
    }
}

void SceneObjectHierarchy::DragAndDropObject(ObjectItem *objItem) {
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        ImGui::SetDragDropPayload("DND_OBJECT3D", &objItem, sizeof(ObjectItem *));
        ImGui::Text("%s", objItem->name.c_str());
        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
        ImGuiDragDropFlags targetFlags = ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("DND_OBJECT3D", targetFlags)) {
            DropPosition dropPosition = DragAndDropTargetCommon();
            // ドロップ処理
            if (payload->IsDelivery()) {
                IM_ASSERT(payload->DataSize == sizeof(ObjectItem *));
                dragDropPayload3D_.objectItemSource = *(ObjectItem **)payload->Data;
                dragDropPayload3D_.objectItemTarget = objItem;
                dragDropPayload3D_.position = dropPosition;
            }
        }
        ImGui::EndDragDropTarget();
    }
}

void SceneObjectHierarchy::ApplyDragAndDrop2D() {
    if (dragDropPayload2D_.objectItemSource && dragDropPayload2D_.objectItemTarget) {
        Object2DItem *sourceItem = dragDropPayload2D_.objectItemSource;
        Object2DItem *targetItem = dragDropPayload2D_.objectItemTarget;
        DropPosition position = dragDropPayload2D_.position;

        // ドロップ処理
        size_t insertIndex = targetItem->originalIndex;
        auto *targetParent = targetItem->object->GetComponent2D<Transform2D>()->GetParentObject();
        auto *sourceTransform = sourceItem->object->GetComponent2D<Transform2D>();
        bool isParentSet = false;
        switch (position) {
            case DropPosition::Above:
                isParentSet = sourceTransform->SetParentObject(targetParent);
                if (isParentSet) {
                    editorContext_->MoveObject2D(sourceItem->object, insertIndex);
                }
                break;
            case DropPosition::Below:
                isParentSet = sourceTransform->SetParentObject(targetParent);
                if (isParentSet) {
                    editorContext_->MoveObject2D(sourceItem->object, insertIndex + 1);
                }
                break;
            case DropPosition::Inside:
                isParentSet = sourceTransform->SetParentObject(targetItem->object);
                if (isParentSet) {
                    editorContext_->MoveObject2D(sourceItem->object, insertIndex);
                }
                break;
            default:
                break;
        }
        dragDropPayload2D_.objectItemSource = nullptr;
        dragDropPayload2D_.objectItemTarget = nullptr;
    }
}

void SceneObjectHierarchy::ApplyDragAndDrop3D() {
    if (dragDropPayload3D_.objectItemSource && dragDropPayload3D_.objectItemTarget) {
        ObjectItem *sourceItem = dragDropPayload3D_.objectItemSource;
        ObjectItem *targetItem = dragDropPayload3D_.objectItemTarget;
        DropPosition position = dragDropPayload3D_.position;
        // ドロップ処理
        size_t moveIndex = targetItem->originalIndex;
        auto *targetParent = targetItem->object->GetComponent3D<Transform3D>()->GetParentObject();
        auto *sourceTransform = sourceItem->object->GetComponent3D<Transform3D>();
        bool isParentSet = false;
        switch (position) {
            case DropPosition::Above:
                isParentSet = sourceTransform->SetParentObject(targetParent);
                if (isParentSet) {
                    editorContext_->MoveObject(sourceItem->object, moveIndex);
                }
                break;
            case DropPosition::Below:
                isParentSet = sourceTransform->SetParentObject(targetParent);
                if (isParentSet) {
                    editorContext_->MoveObject(sourceItem->object, moveIndex + 1);
                }
                break;
            case DropPosition::Inside:
                isParentSet = sourceTransform->SetParentObject(targetItem->object);
                if (isParentSet) {
                    editorContext_->MoveObject(sourceItem->object, moveIndex);
                }
                break;
            default:
                break;
        }
        dragDropPayload3D_.objectItemSource = nullptr;
        dragDropPayload3D_.objectItemTarget = nullptr;
    }
}

SceneObjectHierarchy::DropPosition SceneObjectHierarchy::DragAndDropTargetCommon() {
    // アイテムの矩形とマウスのY座標を取得
    ImVec2 itemRectMin = ImGui::GetItemRectMin();
    ImVec2 itemRectMax = ImGui::GetItemRectMax();
    float mouseY = ImGui::GetMousePos().y;
    float itemHeight = itemRectMax.y - itemRectMin.y;

    // 判定のしきい値
    const float thresholdUpper = itemRectMin.y + itemHeight * 0.25f; // 上側25%
    const float thresholdLower = itemRectMax.y - itemHeight * 0.25f; // 下側25%
    DropPosition dropPosition;

    // マウスの位置に応じてドロップ位置を決定
    if (mouseY < thresholdUpper) {
        dropPosition = DropPosition::Above;
    } else if (mouseY > thresholdLower) {
        dropPosition = DropPosition::Below;
    } else {
        dropPosition = DropPosition::Inside;
    }

    // 視覚的フィードバック
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    ImU32 highlightColor = IM_COL32(0, 255, 0, 255);
    float lineThickness = 2.0f;

    if (dropPosition == DropPosition::Above) {
        drawList->AddLine(ImVec2(itemRectMin.x, itemRectMin.y), ImVec2(itemRectMax.x, itemRectMin.y), highlightColor, lineThickness);
    } else if (dropPosition == DropPosition::Below) {
        drawList->AddLine(ImVec2(itemRectMin.x, itemRectMax.y), ImVec2(itemRectMax.x, itemRectMax.y), highlightColor, lineThickness);
    } else if (dropPosition == DropPosition::Inside) {
        drawList->AddRect(itemRectMin, itemRectMax, highlightColor, 0.0f, 0, lineThickness);
    }

    return dropPosition;
}

} // namespace KashipanEngine