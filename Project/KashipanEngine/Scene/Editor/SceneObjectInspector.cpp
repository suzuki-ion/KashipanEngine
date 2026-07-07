#include "SceneObjectInspector.h"
#include "ComponentSerialize/ComponentRegistry.h"
#include "Scene/Editor/ComponentAddMenu.h"
#include "Scene/Editor/EditorSettings.h"
#include "Scene/Editor/SceneEditorCommands.h"

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

    FlushPendingComponentEdit();
}

void SceneObjectInspector::ShowObjectInspector(EmptyObject *obj) {
    ImGui::Text("Object Inspector");
    ImGui::Separator();

    //--------- 名前（編集セッション終了時にUndo履歴へ積む） ---------//
    std::string name = obj->GetName();
    if (ImGui::InputText("Name", &name)) {
        obj->SetName(name);
    }
    if (ImGui::IsItemActivated()) {
        nameBeforeEdit_ = name;
    }
    if (ImGui::IsItemDeactivatedAfterEdit() && commands_ && obj->GetName() != nameBeforeEdit_) {
        commands_->PushExecuted(std::make_unique<ObjectPropertyCommand>(
            obj, nameBeforeEdit_, obj->GetName(), obj->IsActive(), obj->IsActive()));
    }

    //--------- アクティブ状態 ---------//
    bool active = obj->IsActive();
    if (ImGui::Checkbox("Active", &active)) {
        if (commands_) {
            commands_->Execute(std::make_unique<ObjectPropertyCommand>(
                obj, obj->GetName(), obj->GetName(), obj->IsActive(), active));
        } else {
            obj->SetActive(active);
        }
    }

    //--------- コンポーネント一覧 ---------//
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
        // 開閉状態はコンポーネントの種類ごとに保存される（デフォルトは開いた状態）
        if (EditorSettings::PersistentTreeNode(comp->GetComponentType().c_str(),
                "inspector.component." + comp->GetComponentType())) {
            if (ImGui::BeginPopupContextItem("ComponentContextMenu")) {
                if (ImGui::MenuItem("Remove Component")) {
                    componentToRemove = comp.get();
                }
                ImGui::EndPopup();
            }
            // パラメータ変更をUndo履歴へ積むため、表示前後の状態を比較する
            JSON before = obj->SaveComponentToJson(comp.get());
            obj->ShowComponentImGui(comp.get());
            JSON after = obj->SaveComponentToJson(comp.get());
            if (before != after) {
                TrackComponentEdit(obj, comp.get(), before, after);
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
        ++id;
    }

    if (componentToRemove) {
        if (commands_) {
            commands_->Execute(std::make_unique<RemoveComponentCommand>(obj, componentToRemove));
        } else {
            obj->RemoveComponent(componentToRemove);
        }
    }

    ImGui::Separator();
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup")) {
        // カテゴリごとのツリーメニューから追加する型を選択する
        std::string selectedType;
        if (ComponentAddMenu::Show(GetRegisteredObjectComponentTypes(),
                [](const std::string &typeName) -> const std::vector<std::string> & { return GetObjectComponentCategory(typeName); },
                selectedType)) {
            if (commands_) {
                commands_->Execute(std::make_unique<AddComponentCommand>(obj, selectedType));
            } else {
                auto newComp = CreateObjectComponentByType(selectedType);
                if (newComp) {
                    obj->AddComponent(std::move(newComp));
                }
            }
        }
        ImGui::EndPopup();
    }
}

void SceneObjectInspector::TrackComponentEdit(EmptyObject *obj, IObjectComponent *component, const JSON &before, const JSON &after) {
    // 別のコンポーネントの編集が始まった場合は先に確定させる
    if (hasPendingEdit_ && (editObject_ != obj || editComponent_ != component)) {
        if (commands_ && editBefore_ != editAfter_) {
            commands_->PushExecuted(std::make_unique<ComponentEditCommand>(editObject_, editComponent_, editBefore_, editAfter_));
        }
        hasPendingEdit_ = false;
    }

    if (!hasPendingEdit_) {
        hasPendingEdit_ = true;
        editObject_ = obj;
        editComponent_ = component;
        editBefore_ = before;
    }
    editAfter_ = after;
}

void SceneObjectInspector::FlushPendingComponentEdit() {
    if (!hasPendingEdit_) return;
    // 編集ウィジェットの操作が終わったタイミングでひとつの操作として確定する
    if (ImGui::IsAnyItemActive()) return;

    if (commands_ && editBefore_ != editAfter_) {
        commands_->PushExecuted(std::make_unique<ComponentEditCommand>(editObject_, editComponent_, editBefore_, editAfter_));
    }
    hasPendingEdit_ = false;
    editObject_ = nullptr;
    editComponent_ = nullptr;
    editBefore_ = JSON();
    editAfter_ = JSON();
}

} // namespace KashipanEngine
