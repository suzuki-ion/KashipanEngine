#include "Objects/Components/SceneVariableApplier.h"

#include "Objects/ObjectContext.h"
#include "Scene/SceneContext.h"

namespace KashipanEngine {

std::unique_ptr<IObjectComponent> SceneVariableApplier::Clone() const {
    auto ptr = std::make_unique<SceneVariableApplier>();
    ptr->variableName_ = variableName_;
    ptr->variableScope_ = variableScope_;
    ptr->bindings_ = bindings_;
    return ptr;
}

void SceneVariableApplier::Update() {
    wasApplied_ = false;
    if (variableName_.empty() || bindings_.empty()) return;
    auto *sceneContext = GetOwnerSceneContext();
    if (!sceneContext) return;

    MyAny *variable = (variableScope_ == VariableScope::Global)
        ? sceneContext->GetGlobalSceneVariable(variableName_)
        : sceneContext->GetSceneVariable(variableName_);
    if (!variable || variable->IsEmpty()) return;

    auto *objectContext = GetOwnerObjectContext();
    bool anyApplied = false;
    for (const auto &binding : bindings_) {
        if (ApplyParameterBindingValue(objectContext, binding, *variable, this)) {
            anyApplied = true;
        }
    }
    wasApplied_ = anyApplied;
}

#if defined(USE_IMGUI)

void SceneVariableApplier::ShowImGui() {
    ImGui::InputText("Variable Name", &variableName_);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "読み取るシーン変数名（Scene Variablesウィンドウで追加済みのもの）");
    }

    static const char *kScopeLabels[] = { "Scene", "Global" };
    int scopeIndex = static_cast<int>(variableScope_);
    if (ImGui::Combo("Scope", &scopeIndex, kScopeLabels, IM_ARRAYSIZE(kScopeLabels))) {
        variableScope_ = static_cast<VariableScope>(scopeIndex);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "Scene: 現在のシーン内のシーン変数 / Global: シーンをまたいで保持されるグローバルシーン変数");
    }

    auto *sceneContext = GetOwnerSceneContext();
    MyAny *variable = (sceneContext && !variableName_.empty())
        ? ((variableScope_ == VariableScope::Global) ? sceneContext->GetGlobalSceneVariable(variableName_) : sceneContext->GetSceneVariable(variableName_))
        : nullptr;
    if (variable) {
        ImGui::Text("Type: %s", variable->GetTypeInfo().ToString().c_str());
    } else if (!variableName_.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Variable \"%s\" not found", variableName_.c_str());
    }

    if (wasApplied_) {
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "Applied");
    } else {
        ImGui::TextDisabled("Not Applied");
    }

    ImGui::Separator();
    // 変数が見つからない間はfloat扱いの候補（既定）を出しておく
    const TypeInfo sourceType = variable ? variable->GetTypeInfo() : TypeInfo(ValueType::Float);
    const auto candidates = CollectParameterBindingCandidatesForType(GetOwnerObjectContext(), this, sourceType);
    ShowParameterBindingListImGui(bindings_, candidates);
}

#endif // USE_IMGUI

JSON SceneVariableApplier::SaveToJson() const {
    JSON json = JSON::object();
    json["variableName"] = variableName_;
    json["variableScope"] = static_cast<int>(variableScope_);
    JSON bindingsJson = JSON::array();
    for (const auto &binding : bindings_) {
        bindingsJson.push_back(SaveParameterBindingToJson(binding));
    }
    json["bindings"] = std::move(bindingsJson);
    return json;
}

bool SceneVariableApplier::LoadFromJson(const JSON &json) {
    bindings_.clear();
    variableName_ = json.value("variableName", std::string{});
    variableScope_ = static_cast<VariableScope>(json.value("variableScope", 0));
    if (json.contains("bindings") && json["bindings"].is_array()) {
        for (const auto &bindingJson : json["bindings"]) {
            if (!bindingJson.is_object()) continue;
            bindings_.push_back(LoadParameterBindingFromJson(bindingJson));
        }
    }
    return true;
}

} // namespace KashipanEngine
