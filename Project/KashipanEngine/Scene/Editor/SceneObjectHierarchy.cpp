#include "SceneObjectHierarchy.h"
#include "Objects/Components/2D/Transform2D.h"
#include "Objects/Components/3D/Transform3D.h"

namespace KashipanEngine {

void SceneObjectHierarchy::ShowImGui() {
    RebuildObject2DItems();
    RebuildObject3DItems();

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
            ShowObject3DItem(object3DItems_[i], index);
            ++index;
        }
    }

    ImGui::End();
}

void SceneObjectHierarchy::RebuildObject2DItems() {
    const auto &objects2D = editorContext_->GetObjects2D();
    object2DItems_.clear();
    object2DParentMap_.clear();
    object2DItems_.reserve(objects2D.size());
    object2DParentMap_.reserve(objects2D.size());

    // 親から子を辿れるように、親子関係を構築する
    for (const auto &obj : objects2D) {
        if (!obj) continue;
        object2DParentMap_.emplace(obj.get(), std::vector<Object2DBase *>{});
        auto *transform = obj->GetComponent2D<Transform2D>();
        auto *parent = transform ? transform->GetParentObject() : nullptr;
        if (parent) {
            object2DParentMap_[parent].push_back(obj.get());
        } else {
            // 親がいない場合はトップレベルのアイテムとして追加
            Object2DItem item{};
            item.object = obj.get();
            item.name = obj->GetName();
            item.depth = 0;
            object2DItems_.push_back(std::move(item));
        }
    }

    // 子アイテムを再帰的に追加する
    for (auto &item : object2DItems_) {
        RecursivelyBuildObject2DItems(item.object, item, 0);
    }
}

void SceneObjectHierarchy::RebuildObject3DItems() {
    const auto &objects3D = editorContext_->GetObjects3D();
    object3DItems_.clear();
    object3DParentMap_.clear();
    object3DItems_.reserve(objects3D.size());
    object3DParentMap_.reserve(objects3D.size());

    // 親から子を辿れるように、親子関係を構築する
    for (const auto &obj : objects3D) {
        if (!obj) continue;
        object3DParentMap_.emplace(obj.get(), std::vector<Object3DBase *>{});
        auto *transform = obj->GetComponent3D<Transform3D>();
        auto *parent = transform ? transform->GetParentObject() : nullptr;
        if (parent) {
            object3DParentMap_[parent].push_back(obj.get());
        } else {
            // 親がいない場合はトップレベルのアイテムとして追加
            Object3DItem item{};
            item.object = obj.get();
            item.name = obj->GetName();
            item.depth = 0;
            object3DItems_.push_back(std::move(item));
        }
    }

    // 子アイテムを再帰的に追加する
    for (auto &item : object3DItems_) {
        RecursivelyBuildObject3DItems(item.object, item, 0);
    }
}

void SceneObjectHierarchy::RecursivelyBuildObject2DItems(Object2DBase *obj, Object2DItem &item, size_t depth) {
    for (auto *child : object2DParentMap_[obj]) {
        Object2DItem childItem{};
        childItem.object = child;
        childItem.name = child->GetName();
        childItem.depth = depth + 1;
        item.children.push_back(std::move(childItem));
        RecursivelyBuildObject2DItems(child, item.children.back(), depth + 1);
    }
}

void SceneObjectHierarchy::RecursivelyBuildObject3DItems(Object3DBase *obj, Object3DItem &item, size_t depth) {
    for (auto *child : object3DParentMap_[obj]) {
        Object3DItem childItem{};
        childItem.object = child;
        childItem.name = child->GetName();
        childItem.depth = depth + 1;
        item.children.push_back(std::move(childItem));
        RecursivelyBuildObject3DItems(child, item.children.back(), depth + 1);
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
        if (ImGui::IsItemClicked()) {
            selectedObjectType_ = SelectedObjectType::Object2D;
            selectedObject2DIndex_ = index;
            selectedObject3DIndex_ = SIZE_MAX;
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
        if (ImGui::IsItemClicked()) {
            selectedObjectType_ = SelectedObjectType::Object2D;
            selectedObject2DIndex_ = index;
            selectedObject3DIndex_ = SIZE_MAX;
            selectedObject2D_ = item.object;
        }
    }
    // インデントを戻す
    for (size_t i = 0; i < item.depth; ++i) {
        ImGui::Unindent();
    }
    ImGui::PopID();
}

void SceneObjectHierarchy::ShowObject3DItem(const Object3DItem &item, size_t &index) {
    ImGui::PushID(static_cast<int>(index));
    // 深さの分だけインデントを追加
    for (size_t i = 0; i < item.depth; ++i) {
        ImGui::Indent();
    }
    // 子アイテムがある場合はツリーノードとして表示
    if (!item.children.empty()) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (selectedObjectType_ == SelectedObjectType::Object3D && selectedObject3DIndex_ == index) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        bool isOpen = ImGui::TreeNodeEx(item.name.c_str(), flags);
        if (ImGui::IsItemClicked()) {
            selectedObjectType_ = SelectedObjectType::Object3D;
            selectedObject3DIndex_ = index;
            selectedObject2DIndex_ = SIZE_MAX;
            selectedObject3D_ = item.object;
        }
        if (isOpen) {
            for (const auto &child : item.children) {
                ShowObject3DItem(child, ++index);
            }
            ImGui::TreePop();
        }
    } else {
        // 子アイテムがない場合は通常のテキストとして表示
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (selectedObjectType_ == SelectedObjectType::Object3D && selectedObject3DIndex_ == index) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        ImGui::TreeNodeEx(item.name.c_str(), flags);
        if (ImGui::IsItemClicked()) {
            selectedObjectType_ = SelectedObjectType::Object3D;
            selectedObject3DIndex_ = index;
            selectedObject2DIndex_ = SIZE_MAX;
            selectedObject3D_ = item.object;
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

void SceneObjectHierarchy::ShowObject3DContextMenu(Object3DBase *obj) {
    if (ImGui::BeginPopupContextItem("Object3DContextMenu")) {
        if (ImGui::MenuItem("Delete Object")) {
            editorContext_->RemoveObject3D(obj);
            selectedObjectType_ = SelectedObjectType::None;
            selectedObject3DIndex_ = SIZE_MAX;
            selectedObject3D_ = nullptr;
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

void SceneObjectHierarchy::ShowAddObject3DMenu(Object3DBase *parent) {
    (void)parent;
}

} // namespace KashipanEngine