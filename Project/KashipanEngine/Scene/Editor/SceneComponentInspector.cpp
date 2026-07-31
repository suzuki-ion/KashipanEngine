#include "SceneComponentInspector.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_stdlib.h>

#include "Scene/Editor/ComponentAddMenu.h"
#include "Scene/Editor/EditorSettings.h"
#include "ComponentSerialize/ComponentRegistry.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

void SceneComponentInspector::ShowImGui() {
    if (!context_) return;
    if (!ImGui::Begin(TranslationLabel("editor.scenecomponentinspector.window"))) {
        ImGui::End();
        return;
    }

    ISceneComponent *componentToRemove = nullptr;
    int id = 0;
    for (const auto &entry : context_->GetAllComponents()) {
        const auto &component = entry.first;
        if (!component) continue;

        ImGui::PushID(id);
        ImGui::Separator();
        bool componentActive = component->IsActive();
        if (ImGui::Checkbox("##Active", &componentActive)) {
            component->SetActive(componentActive);
        }
        ImGui::SameLine();
        // 開閉状態はコンポーネントの種類ごとに保存される（デフォルトは開いた状態）
        if (EditorSettings::PersistentTreeNode(component->GetComponentType().c_str(),
                "sceneComponentInspector." + component->GetComponentType())) {
            if (ImGui::BeginPopupContextItem("SceneComponentContextMenu")) {
                if (ImGui::MenuItem(TranslationLabel("editor.component.remove"))) {
                    componentToRemove = component.get();
                }
                ImGui::EndPopup();
            }
            // タグ（分類・判別用の任意文字列）
            std::string tagName = component->GetTagName();
            if (ImGui::InputText(TranslationLabel("editor.component.tag"), &tagName)) {
                component->SetTag(tagName);
            }
            context_->ShowComponentImGui(component.get());
            ImGui::TreePop();
        }
        ImGui::PopID();
        ++id;
    }

    if (componentToRemove) {
        context_->RemoveComponent(componentToRemove);
    }

    ImGui::Separator();
    if (ImGui::Button(TranslationLabel("editor.scenecomponentinspector.add"))) {
        ImGui::OpenPopup("AddSceneComponentPopup");
    }

    if (ImGui::BeginPopup("AddSceneComponentPopup")) {
        // カテゴリごとのツリーメニューから追加する型を選択する
        std::string selectedType;
        if (ComponentAddMenu::Show(GetRegisteredSceneComponentTypes(),
                [](const std::string &typeName) -> const std::vector<std::string> & { return GetSceneComponentCategory(typeName); },
                selectedType)) {
            auto newComponent = CreateSceneComponentByType(selectedType);
            if (newComponent) {
                context_->AddComponent(std::move(newComponent));
            }
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

} // namespace KashipanEngine

#endif // USE_IMGUI
