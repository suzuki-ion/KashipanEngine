#include "SceneObjectInspector.h"
#include "ComponentSerialize/ComponentRegistry.h"

namespace KashipanEngine {

void SceneObjectInspector::ShowImGui() {
    ImGui::Begin("Scene Object Inspector");
    if (objectHierarchy_) {
        EmptyObject *selectedObject = objectHierarchy_->GetSelectedObject();
        if (selectedObject) {
            ShowObjectInspector(selectedObject);
        } else {
            ImGui::Text("No object selected.");
        }
    } else {
        ImGui::Text("No object hierarchy available.");
    }
    ImGui::End();
}

void SceneObjectInspector::ShowObjectInspector(EmptyObject *obj) {
    ImGui::Text("Object Inspector");
    ImGui::Separator();

    std::string name = obj->GetName();
    if (ImGui::InputText("Name", &name)) {
        obj->SetName(name);
    }

    bool active = obj->IsActive();
    if (ImGui::Checkbox("Active", &active)) {
        obj->SetActive(active);
    }

    IObjectComponent *componentToRemove = nullptr;
    int id = 0;
    for (const auto &entry : obj->GetAllComponents()) {
        const auto &comp = entry.first;
        if (!comp) continue;

        ImGui::PushID(id);
        ImGui::Separator();
        bool componentActive = comp->IsActive();
        if (ImGui::Checkbox("##Active", &componentActive)) {
            comp->SetActive(componentActive);
        }
        ImGui::SameLine();
        if (ImGui::TreeNode(comp->GetComponentType().c_str())) {
            if (ImGui::BeginPopupContextItem("ComponentContextMenu")) {
                if (ImGui::MenuItem("Remove Component")) {
                    componentToRemove = comp.get();
                }
                ImGui::EndPopup();
            }
            obj->ShowComponentImGui(comp.get());
            ImGui::TreePop();
        }
        ImGui::PopID();
        ++id;
    }

    if (componentToRemove) {
        obj->RemoveComponent(componentToRemove);
    }

    ImGui::Separator();
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup")) {
        for (const auto &compType : GetRegisteredObjectComponentTypes()) {
            if (ImGui::MenuItem(compType.c_str())) {
                auto newComp = CreateObjectComponentByType(compType);
                if (newComp) {
                    obj->AddComponent(std::move(newComp));
                }
            }
        }
        ImGui::EndPopup();
    }
}

} // namespace KashipanEngine
