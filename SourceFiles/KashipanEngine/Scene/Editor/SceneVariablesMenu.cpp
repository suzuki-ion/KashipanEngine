#include "SceneVariablesMenu.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include "Scene/Editor/EditorWindowChrome.h"
#include "Utilities/ImGuiCustom.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

void SceneVariablesMenu::ShowImGui() {
    if (!context_) return;
    if (!ImGui::Begin(TranslationLabel("editor.scenevariables.window"))) {
        ImGui::End();
        return;
    }
    DrawFloatingWindowChromeButtons();

    //--------- 変数の追加 ---------//
    ImGui::InputText("##NewVariableName", &newVariableName_);
    ImGui::SameLine();
    static const char *kTypeNames[] = { "Bool", "Int", "Float", "Double", "String", "Vector2", "Vector3", "Vector4", "Quaternion", "Matrix4x4" };
    ImGui::SetNextItemWidth(120.0f);
    ImGui::Combo("##NewVariableType", &newVariableType_, kTypeNames, IM_ARRAYSIZE(kTypeNames));
    ImGui::SameLine();
    if (ImGui::Button(TranslationLabel("editor.common.add")) && !newVariableName_.empty() && !context_->GetSceneVariable(newVariableName_)) {
        AddVariableOfSelectedType(newVariableName_);
        newVariableName_.clear();
    }

    ImGui::Separator();

    //--------- 変数一覧と編集・削除 ---------//
    std::string pendingRemoveKey;
    int id = 0;
    for (const auto &pair : context_->GetSceneVariables()) {
        const std::string &key = pair.first;
        MyAny *variable = context_->GetSceneVariable(key);
        if (!variable) continue;

        ImGui::PushID(id++);
        ShowVariableEditor(key, variable);
        ImGui::SameLine();
        if (ImGui::SmallButton(TranslationLabel("editor.common.remove"))) {
            pendingRemoveKey = key;
        }
        ImGui::PopID();
    }
    if (!pendingRemoveKey.empty()) {
        context_->RemoveSceneVariable(pendingRemoveKey);
    }

    ImGui::End();
}

void SceneVariablesMenu::AddVariableOfSelectedType(const std::string &key) {
    switch (newVariableType_) {
    case 0: context_->AddSceneVariable<bool>(key, false); break;
    case 1: context_->AddSceneVariable<int>(key, 0); break;
    case 2: context_->AddSceneVariable<float>(key, 0.0f); break;
    case 3: context_->AddSceneVariable<double>(key, 0.0); break;
    case 4: context_->AddSceneVariable<std::string>(key, std::string{}); break;
    case 5: context_->AddSceneVariable<Vector2>(key, Vector2::Zero()); break;
    case 6: context_->AddSceneVariable<Vector3>(key, Vector3::Zero()); break;
    case 7: context_->AddSceneVariable<Vector4>(key, Vector4::Zero()); break;
    case 8: context_->AddSceneVariable<Quaternion>(key, Quaternion::Identity()); break;
    case 9: context_->AddSceneVariable<Matrix4x4>(key, Matrix4x4::Identity()); break;
    default: break;
    }
}

void SceneVariablesMenu::ShowVariableEditor(const std::string &key, MyAny *variable) {
    ImGuiCustom::EditValue(key.c_str(), *variable);
}

} // namespace KashipanEngine

#endif // USE_IMGUI
