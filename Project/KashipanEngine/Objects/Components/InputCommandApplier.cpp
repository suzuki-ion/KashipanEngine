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
    ptr->commands_.reserve(commands_.size());
    for (const auto &entry : commands_) {
        CommandEntry copy;
        copy.name = entry.name;
        copy.commandName = entry.commandName;
        copy.conditionType = entry.conditionType;
        copy.threshold = entry.threshold;
        copy.valueSource = entry.valueSource;
        copy.fixedValue = entry.fixedValue;
        copy.valueScale = entry.valueScale;
        copy.valueOffset = entry.valueOffset;
        copy.bindings = entry.bindings;
        ptr->commands_.push_back(std::move(copy));
    }
    return ptr;
}

bool InputCommandApplier::WasApplied(const std::string &name) const {
    const auto *entry = FindCommand(name);
    return entry && entry->wasApplied;
}

bool InputCommandApplier::TryGetLastValue(const std::string &name, float &outValue) const {
    const auto *entry = FindCommand(name);
    if (!entry) return false;
    outValue = entry->lastValue;
    return true;
}

void InputCommandApplier::Update() {
    auto *sceneContext = GetOwnerSceneContext();
    auto *inputCommand = sceneContext ? sceneContext->GetInputCommand() : nullptr;

    for (auto &entry : commands_) {
        entry.wasApplied = false;
        if (entry.commandName.empty() || entry.bindings.empty() || !inputCommand) continue;

        const auto result = inputCommand->Evaluate(entry.commandName);

        bool conditionMet = false;
        switch (entry.conditionType) {
        case ConditionType::CommandTrue:
            conditionMet = result.Triggered();
            break;
        case ConditionType::CommandFalse:
            conditionMet = !result.Triggered();
            break;
        case ConditionType::ValueGreaterEqual:
            conditionMet = result.Value() >= entry.threshold;
            break;
        case ConditionType::ValueLessEqual:
            conditionMet = result.Value() <= entry.threshold;
            break;
        case ConditionType::ValueEqual:
            conditionMet = std::abs(result.Value() - entry.threshold) <= kValueEqualEpsilon;
            break;
        }
        if (!conditionMet) continue;

        const float sourceValue = (entry.valueSource == ValueSource::FixedValue) ? entry.fixedValue : result.Value();
        const float value = sourceValue * entry.valueScale + entry.valueOffset;
        entry.lastValue = value;
        entry.wasApplied = true;
        ApplyValue(entry, value);
    }
}

InputCommandApplier::CommandEntry *InputCommandApplier::FindCommand(const std::string &name) {
    for (auto &entry : commands_) {
        if (entry.name == name) return &entry;
    }
    return nullptr;
}

const InputCommandApplier::CommandEntry *InputCommandApplier::FindCommand(const std::string &name) const {
    for (const auto &entry : commands_) {
        if (entry.name == name) return &entry;
    }
    return nullptr;
}

void InputCommandApplier::ApplyValue(const CommandEntry &entry, float value) {
    auto *objectContext = GetOwnerObjectContext();
    if (!objectContext) return;
    for (const auto &binding : entry.bindings) {
        ApplyParameterBinding(objectContext, binding, value, this);
    }
}

#if defined(USE_IMGUI)

void InputCommandApplier::ShowImGui() {
    if (ImGui::Button("Add Command")) {
        CommandEntry entry;
        int number = 0;
        std::string newName = "Cmd0";
        while (FindCommand(newName)) {
            ++number;
            newName = "Cmd" + std::to_string(number);
        }
        entry.name = newName;
        commands_.push_back(std::move(entry));
    }

    const auto candidates = CollectParameterBindingCandidates(GetOwnerObjectContext(), this);
    int removeIndex = -1;
    for (int i = 0; i < static_cast<int>(commands_.size()); ++i) {
        auto &entry = commands_[i];
        ImGui::PushID(i);
        // 名前変更でヘッダーの開閉状態が変わらないよう、IDは"###"でインデックス固定にする
        if (ImGui::CollapsingHeader((entry.name + "###commandEntry").c_str())) {
            ShowCommandImGui(entry, candidates);
            if (ImGui::SmallButton("Delete Command")) {
                removeIndex = i;
            }
        }
        ImGui::PopID();
    }
    if (removeIndex >= 0) {
        commands_.erase(commands_.begin() + removeIndex);
    }
}

void InputCommandApplier::ShowCommandImGui(CommandEntry &entry, const std::vector<ParameterBindingCandidate> &candidates) {
    ImGui::InputText("Name", &entry.name);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "WasApplied/GetLastValue等をコードから呼ぶ際に指定する識別名");
    }
    ImGui::InputText("Command Name", &entry.commandName);
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
    int conditionIndex = static_cast<int>(entry.conditionType);
    if (ImGui::Combo("Condition", &conditionIndex, kConditionLabels, IM_ARRAYSIZE(kConditionLabels))) {
        entry.conditionType = static_cast<ConditionType>(conditionIndex);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "値を適用する条件。条件を満たしたフレームだけ書き込みが行われる");
    }
    if (entry.conditionType == ConditionType::ValueGreaterEqual ||
        entry.conditionType == ConditionType::ValueLessEqual ||
        entry.conditionType == ConditionType::ValueEqual) {
        ImGui::DragFloat("Threshold", &entry.threshold, 0.01f);
        if (ImGui::IsItemHovered() && entry.conditionType == ConditionType::ValueEqual) {
            ImGui::SetTooltip("%s", "比較する閾値（==は誤差1e-6以内で判定する）");
        }
    }

    static const char *kSourceLabels[] = {
        "Command Value",
        "Fixed Value",
    };
    int sourceIndex = static_cast<int>(entry.valueSource);
    if (ImGui::Combo("Value Source", &sourceIndex, kSourceLabels, IM_ARRAYSIZE(kSourceLabels))) {
        entry.valueSource = static_cast<ValueSource>(sourceIndex);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "書き込む値の種類（入力コマンドの評価値 / 固定値）");
    }
    if (entry.valueSource == ValueSource::FixedValue) {
        ImGui::DragFloat("Fixed Value", &entry.fixedValue, 0.01f);
    }
    ImGui::DragFloat("Value Scale", &entry.valueScale, 0.01f);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "書き込む値にかけるスケール。Value Offsetの加算より先に適用される（適用値 = 値 × Scale + Offset）");
    }
    ImGui::DragFloat("Value Offset", &entry.valueOffset, 0.01f);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "書き込む値へ加算するデフォルト値オフセット");
    }

    if (entry.wasApplied) {
        ImGui::Text("Applied: %.3f", entry.lastValue);
    } else {
        ImGui::TextDisabled("Not Applied");
    }

    ImGui::Separator();
    ShowParameterBindingListImGui(entry.bindings, candidates);
    ImGui::Separator();
}

#endif // USE_IMGUI

JSON InputCommandApplier::SaveToJson() const {
    JSON json = JSON::object();
    JSON commandsJson = JSON::array();
    for (const auto &entry : commands_) {
        JSON entryJson = JSON::object();
        entryJson["name"] = entry.name;
        entryJson["commandName"] = entry.commandName;
        entryJson["conditionType"] = static_cast<int>(entry.conditionType);
        entryJson["threshold"] = entry.threshold;
        entryJson["valueSource"] = static_cast<int>(entry.valueSource);
        entryJson["fixedValue"] = entry.fixedValue;
        entryJson["valueScale"] = entry.valueScale;
        entryJson["valueOffset"] = entry.valueOffset;
        JSON bindingsJson = JSON::array();
        for (const auto &binding : entry.bindings) {
            bindingsJson.push_back(SaveParameterBindingToJson(binding));
        }
        entryJson["bindings"] = std::move(bindingsJson);
        commandsJson.push_back(std::move(entryJson));
    }
    json["commands"] = std::move(commandsJson);
    return json;
}

bool InputCommandApplier::LoadFromJson(const JSON &json) {
    commands_.clear();
    if (!json.contains("commands") || !json["commands"].is_array()) {
        return true;
    }
    for (const auto &entryJson : json["commands"]) {
        if (!entryJson.is_object()) continue;
        CommandEntry entry;
        entry.name = entryJson.value("name", std::string{});
        entry.commandName = entryJson.value("commandName", std::string{});
        entry.conditionType = static_cast<ConditionType>(entryJson.value("conditionType", 0));
        entry.threshold = entryJson.value("threshold", 0.5f);
        entry.valueSource = static_cast<ValueSource>(entryJson.value("valueSource", 0));
        entry.fixedValue = entryJson.value("fixedValue", 1.0f);
        entry.valueScale = entryJson.value("valueScale", 1.0f);
        entry.valueOffset = entryJson.value("valueOffset", 0.0f);
        if (entryJson.contains("bindings") && entryJson["bindings"].is_array()) {
            for (const auto &bindingJson : entryJson["bindings"]) {
                if (!bindingJson.is_object()) continue;
                entry.bindings.push_back(LoadParameterBindingFromJson(bindingJson));
            }
        }
        commands_.push_back(std::move(entry));
    }
    return true;
}

} // namespace KashipanEngine
