#include "GlobalSceneVariablesMenu.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include "Scene/Editor/EditorWindowChrome.h"
#include "Scene/SceneManager.h"
#include "Debug/Logger.h"
#include "Utilities/ImGuiCustom.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

void GlobalSceneVariablesMenu::ShowImGui() {
    LogScope scope;
    if (!context_) return;
    if (!ImGui::Begin(TranslationLabel("editor.globalscenevariables.window"))) {
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
    if (ImGui::Button(TranslationLabel("editor.common.add")) && !newVariableName_.empty() && !context_->GetGlobalSceneVariable(newVariableName_)) {
        AddVariableOfSelectedType(newVariableName_);
        newVariableName_.clear();
        SaveToFile();
    }

    ImGui::Separator();

    //--------- 変数一覧と編集・削除 ---------//
    std::string pendingRemoveKey;
    bool changed = false;
    int id = 0;
    for (const auto &pair : context_->GetGlobalSceneVariables()) {
        const std::string &key = pair.first;
        MyAny *variable = context_->GetGlobalSceneVariable(key);
        if (!variable) continue;

        ImGui::PushID(id++);
        if (ShowVariableEditor(key, variable)) {
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(TranslationLabel("editor.common.remove"))) {
            pendingRemoveKey = key;
        }
        ImGui::PopID();
    }
    if (!pendingRemoveKey.empty()) {
        context_->RemoveGlobalSceneVariable(pendingRemoveKey);
        changed = true;
    }
    if (changed) {
        SaveToFile();
    }

    ImGui::End();
}

void GlobalSceneVariablesMenu::AddVariableOfSelectedType(const std::string &key) {
    LogScope scope;
    switch (newVariableType_) {
    case 0: context_->AddGlobalSceneVariable<bool>(key, false); break;
    case 1: context_->AddGlobalSceneVariable<int>(key, 0); break;
    case 2: context_->AddGlobalSceneVariable<float>(key, 0.0f); break;
    case 3: context_->AddGlobalSceneVariable<double>(key, 0.0); break;
    case 4: context_->AddGlobalSceneVariable<std::string>(key, std::string{}); break;
    case 5: context_->AddGlobalSceneVariable<Vector2>(key, Vector2::Zero()); break;
    case 6: context_->AddGlobalSceneVariable<Vector3>(key, Vector3::Zero()); break;
    case 7: context_->AddGlobalSceneVariable<Vector4>(key, Vector4::Zero()); break;
    case 8: context_->AddGlobalSceneVariable<Quaternion>(key, Quaternion::Identity()); break;
    case 9: context_->AddGlobalSceneVariable<Matrix4x4>(key, Matrix4x4::Identity()); break;
    default: break;
    }
}

bool GlobalSceneVariablesMenu::ShowVariableEditor(const std::string &key, MyAny *variable) {
    LogScope scope;
    return ImGuiCustom::EditValue(key.c_str(), *variable);
}

void GlobalSceneVariablesMenu::SaveToFile() {
    LogScope scope;
    if (auto *sceneManager = context_->GetSceneManager()) {
        sceneManager->SaveGlobalSceneVariables();
    }
}

} // namespace KashipanEngine

#endif // USE_IMGUI
