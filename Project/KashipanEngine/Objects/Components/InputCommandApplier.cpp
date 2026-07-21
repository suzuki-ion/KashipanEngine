#include "Objects/Components/InputCommandApplier.h"

#include <cmath>

#include "Input/InputCommand.h"
#include "Objects/ObjectContext.h"
#include "Scene/SceneContext.h"

namespace KashipanEngine {

namespace {

/// @brief ValueEqual条件で「等しい」とみなす許容誤差
constexpr float kValueEqualEpsilon = 1e-6f;

} // namespace

std::unique_ptr<IObjectComponent> InputCommandApplier::Clone() const {
    auto ptr = std::make_unique<InputCommandApplier>();
    ptr->commandName_ = commandName_;
    ptr->conditionType_ = conditionType_;
    ptr->threshold_ = threshold_;
    ptr->valueSource_ = valueSource_;
    ptr->fixedValue_ = fixedValue_;
    ptr->valueScale_ = valueScale_;
    ptr->valueOffset_ = valueOffset_;
    ptr->bindings_ = bindings_;
    return ptr;
}

void InputCommandApplier::Update() {
    wasApplied_ = false;
    if (commandName_.empty() || bindings_.empty()) return;
    auto *sceneContext = GetOwnerSceneContext();
    auto *inputCommand = sceneContext ? sceneContext->GetInputCommand() : nullptr;
    if (!inputCommand) return;

    const auto result = inputCommand->Evaluate(commandName_);

    bool conditionMet = false;
    switch (conditionType_) {
    case ConditionType::CommandTrue:
        conditionMet = result.Triggered();
        break;
    case ConditionType::CommandFalse:
        conditionMet = !result.Triggered();
        break;
    case ConditionType::ValueGreaterEqual:
        conditionMet = result.Value() >= threshold_;
        break;
    case ConditionType::ValueLessEqual:
        conditionMet = result.Value() <= threshold_;
        break;
    case ConditionType::ValueEqual:
        conditionMet = std::abs(result.Value() - threshold_) <= kValueEqualEpsilon;
        break;
    }
    if (!conditionMet) return;

    const float sourceValue = (valueSource_ == ValueSource::FixedValue) ? fixedValue_ : result.Value();
    const float value = sourceValue * valueScale_ + valueOffset_;
    lastValue_ = value;
    wasApplied_ = true;

    auto *objectContext = GetOwnerObjectContext();
    for (const auto &binding : bindings_) {
        ApplyParameterBinding(objectContext, binding, value, this);
    }
}

#if defined(USE_IMGUI)

void InputCommandApplier::ShowImGui() {
    ImGui::InputText("Command Name", &commandName_);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "評価する入力コマンド名（InputCommandに登録済みのコマンド）");
    }

    static const char *kConditionLabels[] = {
        "Command == true",
        "Command == false",
        "Value >= Threshold",
        "Value <= Threshold",
        "Value == Threshold",
    };
    int conditionIndex = static_cast<int>(conditionType_);
    if (ImGui::Combo("Condition", &conditionIndex, kConditionLabels, IM_ARRAYSIZE(kConditionLabels))) {
        conditionType_ = static_cast<ConditionType>(conditionIndex);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "値を適用する条件。条件を満たしたフレームだけ書き込みが行われる");
    }
    if (conditionType_ == ConditionType::ValueGreaterEqual ||
        conditionType_ == ConditionType::ValueLessEqual ||
        conditionType_ == ConditionType::ValueEqual) {
        ImGui::DragFloat("Threshold", &threshold_, 0.01f);
        if (ImGui::IsItemHovered() && conditionType_ == ConditionType::ValueEqual) {
            ImGui::SetTooltip("%s", "比較する閾値（==は誤差1e-6以内で判定する）");
        }
    }

    static const char *kSourceLabels[] = {
        "Command Value",
        "Fixed Value",
    };
    int sourceIndex = static_cast<int>(valueSource_);
    if (ImGui::Combo("Value Source", &sourceIndex, kSourceLabels, IM_ARRAYSIZE(kSourceLabels))) {
        valueSource_ = static_cast<ValueSource>(sourceIndex);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "書き込む値の種類（入力コマンドの評価値 / 固定値）");
    }
    if (valueSource_ == ValueSource::FixedValue) {
        ImGui::DragFloat("Fixed Value", &fixedValue_, 0.01f);
    }
    ImGui::DragFloat("Value Scale", &valueScale_, 0.01f);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "書き込む値にかけるスケール。Value Offsetの加算より先に適用される（適用値 = 値 × Scale + Offset）");
    }
    ImGui::DragFloat("Value Offset", &valueOffset_, 0.01f);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "書き込む値へ加算するデフォルト値オフセット");
    }

    if (wasApplied_) {
        ImGui::Text("Applied: %.3f", lastValue_);
    } else {
        ImGui::TextDisabled("Not Applied");
    }

    ImGui::Separator();
    const auto candidates = CollectParameterBindingCandidates(GetOwnerObjectContext(), this);
    ShowParameterBindingListImGui(bindings_, candidates);
}

#endif // USE_IMGUI

JSON InputCommandApplier::SaveToJson() const {
    JSON json = JSON::object();
    json["commandName"] = commandName_;
    json["conditionType"] = static_cast<int>(conditionType_);
    json["threshold"] = threshold_;
    json["valueSource"] = static_cast<int>(valueSource_);
    json["fixedValue"] = fixedValue_;
    json["valueScale"] = valueScale_;
    json["valueOffset"] = valueOffset_;
    JSON bindingsJson = JSON::array();
    for (const auto &binding : bindings_) {
        bindingsJson.push_back(SaveParameterBindingToJson(binding));
    }
    json["bindings"] = std::move(bindingsJson);
    return json;
}

bool InputCommandApplier::LoadFromJson(const JSON &json) {
    bindings_.clear();
    commandName_ = json.value("commandName", std::string{});
    conditionType_ = static_cast<ConditionType>(json.value("conditionType", 0));
    threshold_ = json.value("threshold", 0.5f);
    valueSource_ = static_cast<ValueSource>(json.value("valueSource", 0));
    fixedValue_ = json.value("fixedValue", 1.0f);
    valueScale_ = json.value("valueScale", 1.0f);
    valueOffset_ = json.value("valueOffset", 0.0f);
    if (json.contains("bindings") && json["bindings"].is_array()) {
        for (const auto &bindingJson : json["bindings"]) {
            if (!bindingJson.is_object()) continue;
            bindings_.push_back(LoadParameterBindingFromJson(bindingJson));
        }
    }
    return true;
}

} // namespace KashipanEngine
