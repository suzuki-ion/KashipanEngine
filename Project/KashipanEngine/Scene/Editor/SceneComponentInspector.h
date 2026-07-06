#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include "Scene/SceneEditorContext.h"
#include "Scene/Editor/EditorSettings.h"
#include "ComponentSerialize/ComponentRegistry.h"

namespace KashipanEngine {

class SceneEditor;

/// @brief シーンコンポーネントのインスペクターウィンドウ
class SceneComponentInspector final {
public:
    SceneComponentInspector(Passkey<SceneEditor>, SceneEditorContext *context) : context_(context) {}
    ~SceneComponentInspector() = default;

    void ShowImGui() {
        if (!context_) return;
        if (!ImGui::Begin("Scene Component Inspector")) {
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
                    if (ImGui::MenuItem("Remove Component")) {
                        componentToRemove = component.get();
                    }
                    ImGui::EndPopup();
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
        if (ImGui::Button("Add Scene Component")) {
            ImGui::OpenPopup("AddSceneComponentPopup");
        }

        if (ImGui::BeginPopup("AddSceneComponentPopup")) {
            for (const auto &componentType : GetRegisteredSceneComponentTypes()) {
                if (ImGui::MenuItem(componentType.c_str())) {
                    auto newComponent = CreateSceneComponentByType(componentType);
                    if (newComponent) {
                        context_->AddComponent(std::move(newComponent));
                    }
                }
            }
            ImGui::EndPopup();
        }

        ImGui::End();
    }

private:
    SceneEditorContext *context_ = nullptr;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
