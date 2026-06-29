#include "SceneObjectInspector.h"
#include "ComponentSerialize/ComponentRegistry.h"

namespace KashipanEngine {

void SceneObjectInspector::ShowImGui() {
    ImGui::Begin("Scene Object Inspector");
    if (objectHierarchy_) {
        Object2DBase *selectedObject2D = objectHierarchy_->GetSelectedObject2D();
        EmptyObject *selectedObject = objectHierarchy_->GetSelectedObject();
        if (selectedObject2D) {
            ShowObject2DInspector(selectedObject2D);
        } else if (selectedObject) {
            ShowObjectInspector(selectedObject);
        } else {
            ImGui::Text("No object selected.");
        }
    } else {
        ImGui::Text("No object hierarchy available.");
    }
    ImGui::End();
}

void SceneObjectInspector::ShowObject2DInspector(Object2DBase *obj) {
    ImGui::Text("Object2D Inspector");
    ImGui::Separator();
    int id = 0;
    IObjectComponent2D *componentToRemove = nullptr; // 削除するコンポーネントを保持する変数
    for (const auto &comp : obj->GetAllComponents2D()) {
        ImGui::PushID(id);
        ImGui::Separator();
        // 開閉可能なツリー構造でコンポーネントを表示
        if (ImGui::TreeNode(comp->GetComponentType().c_str())) {
            // 一番初めのコンポーネントでなければ右クリックでコンテキストメニューを表示
            if (id > 0 && ImGui::BeginPopupContextItem("ComponentContextMenu")) {
                if (ImGui::MenuItem("Remove Component")) {
                    componentToRemove = comp.get();
                }
                ImGui::EndPopup();
            }
            comp->ShowImGui();
            ImGui::TreePop();
        }
        ImGui::Indent();
        ImGui::Unindent();
        ImGui::PopID();
        id++;
    }

    // 削除するコンポーネントがあれば削除する
    if (componentToRemove) {
        obj->RemoveComponent2D(componentToRemove);
    }

    // 追加可能なコンポーネントのリストを表示
    ImGui::Separator();
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup")) {
        for (const auto &compType : GetRegisteredObject2DComponentTypes()) {
            if (ImGui::MenuItem(compType.c_str())) {
                // コンポーネントを追加する処理
                auto newComp = CreateObject2DComponentByType(compType);
                if (newComp) {
                    obj->RegisterComponent(std::move(newComp));
                }
            }
        }
        ImGui::EndPopup();
    }
}

void SceneObjectInspector::ShowObjectInspector(EmptyObject *obj) {
    ImGui::Text("Object Inspector");
    ImGui::Separator();
    ImGui::Text("Name: %s", obj->GetName().c_str());
    int id = 0;
    IObjectComponent *componentToRemove = nullptr; // 削除するコンポーネントを保持する変数
    for (const auto &comp : obj->GetAllComponents3D()) {
        ImGui::PushID(id);
        ImGui::Separator();
        // 開閉可能なツリー構造でコンポーネントを表示
        if (ImGui::TreeNode(comp->GetComponentType().c_str())) {
            // 一番初めのコンポーネントでなければ右クリックでコンテキストメニューを表示
            if (id > 0 && ImGui::BeginPopupContextItem("ComponentContextMenu")) {
                if (ImGui::MenuItem("Remove Component")) {
                    componentToRemove = comp.get();
                }
                ImGui::EndPopup();
            }
            comp->ShowImGui();
            ImGui::TreePop();
        }
        ImGui::Indent();
        ImGui::Unindent();
        ImGui::PopID();
        id++;
    }

    // 削除するコンポーネントがあれば削除する
    if (componentToRemove) {
        obj->RemoveComponent3D(componentToRemove);
    }

    // 追加可能なコンポーネントのリストを表示
    ImGui::Separator();
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentPopup3D");
    }

    if (ImGui::BeginPopup("AddComponentPopup3D")) {
        for (const auto &compType : GetRegisteredObjectComponentTypes()) {
            if (ImGui::MenuItem(compType.c_str())) {
                // コンポーネントを追加する処理
                auto newComp = CreateObjectComponentByType(compType);
                if (newComp) {
                    obj->RegisterComponent(std::move(newComp));
                }
            }
        }
        ImGui::EndPopup();
    }
}

} // namespace KashipanEngine