#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include <string>
#include "Scene/SceneEditorContext.h"
#include "Utilities/ImGuiCustom.h"

namespace KashipanEngine {

class SceneEditor;

/// @brief シーン変数の追加・削除・編集を行うメニューウィンドウ
class SceneVariablesMenu final {
public:
    SceneVariablesMenu(Passkey<SceneEditor>, SceneEditorContext *context) : context_(context) {}
    ~SceneVariablesMenu() = default;

    void ShowImGui() {
        if (!context_) return;
        if (!ImGui::Begin("Scene Variables")) {
            ImGui::End();
            return;
        }

        //--------- 変数の追加 ---------//
        ImGui::InputText("##NewVariableName", &newVariableName_);
        ImGui::SameLine();
        static const char *kTypeNames[] = { "Bool", "Int", "Float", "Double", "String", "Vector2", "Vector3", "Vector4", "Quaternion", "Matrix4x4" };
        ImGui::SetNextItemWidth(120.0f);
        ImGui::Combo("##NewVariableType", &newVariableType_, kTypeNames, IM_ARRAYSIZE(kTypeNames));
        ImGui::SameLine();
        if (ImGui::Button("Add") && !newVariableName_.empty() && !context_->GetSceneVariable(newVariableName_)) {
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
            if (ImGui::SmallButton("Remove")) {
                pendingRemoveKey = key;
            }
            ImGui::PopID();
        }
        if (!pendingRemoveKey.empty()) {
            context_->RemoveSceneVariable(pendingRemoveKey);
        }

        ImGui::End();
    }

private:
    void AddVariableOfSelectedType(const std::string &key) {
        switch (newVariableType_) {
        case 0: context_->AddSceneVariable<bool>(key); break;
        case 1: context_->AddSceneVariable<int>(key); break;
        case 2: context_->AddSceneVariable<float>(key); break;
        case 3: context_->AddSceneVariable<double>(key); break;
        case 4: context_->AddSceneVariable<std::string>(key); break;
        case 5: context_->AddSceneVariable<Vector2>(key); break;
        case 6: context_->AddSceneVariable<Vector3>(key); break;
        case 7: context_->AddSceneVariable<Vector4>(key); break;
        case 8: context_->AddSceneVariable<Quaternion>(key, Quaternion::Identity()); break;
        case 9: context_->AddSceneVariable<Matrix4x4>(key, Matrix4x4::Identity()); break;
        default: break;
        }
    }

    /// @brief 型に応じたシーン変数の編集UI
    void ShowVariableEditor(const std::string &key, MyAny *variable) {
        const auto baseType = variable->GetTypeInfo().GetBaseType();
        const char *label = key.c_str();
        switch (baseType) {
        case ValueType::Bool:
            if (auto *value = variable->AnyCastPtr<bool>()) { ImGuiCustom::EditValue(label, *value); return; }
            break;
        case ValueType::Int32:
            if (auto *value = variable->AnyCastPtr<int>()) { ImGuiCustom::EditValue(label, *value); return; }
            break;
        case ValueType::Float:
            if (auto *value = variable->AnyCastPtr<float>()) { ImGuiCustom::EditValue(label, *value); return; }
            break;
        case ValueType::Double:
            if (auto *value = variable->AnyCastPtr<double>()) { ImGuiCustom::EditValue(label, *value); return; }
            break;
        case ValueType::String:
            if (auto *value = variable->AnyCastPtr<std::string>()) { ImGuiCustom::EditValue(label, *value); return; }
            break;
        case ValueType::Vector2:
            if (auto *value = variable->AnyCastPtr<Vector2>()) { ImGuiCustom::EditValue(label, *value); return; }
            break;
        case ValueType::Vector3:
            if (auto *value = variable->AnyCastPtr<Vector3>()) { ImGuiCustom::EditValue(label, *value); return; }
            break;
        case ValueType::Vector4:
            if (auto *value = variable->AnyCastPtr<Vector4>()) { ImGuiCustom::EditValue(label, *value); return; }
            break;
        case ValueType::Quaternion:
            if (auto *value = variable->AnyCastPtr<Quaternion>()) { ImGuiCustom::EditValue(label, *value); return; }
            break;
        case ValueType::Matrix4x4:
            if (auto *value = variable->AnyCastPtr<Matrix4x4>()) { ImGuiCustom::EditValue(label, *value); return; }
            break;
        default:
            break;
        }
        ImGui::Text("%s : %s (unsupported)", key.c_str(), variable->GetTypeInfo().ToString().c_str());
    }

    SceneEditorContext *context_ = nullptr;
    std::string newVariableName_;
    int newVariableType_ = 2; // 既定は Float
};

} // namespace KashipanEngine

#endif // USE_IMGUI
