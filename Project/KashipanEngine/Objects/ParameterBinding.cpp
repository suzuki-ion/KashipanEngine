#include "Objects/ParameterBinding.h"

#include <algorithm>
#include <unordered_map>

#include "Objects/Components/ScriptComponent.h"
#include "Objects/IObjectComponent.h"
#include "Objects/ObjectContext.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

namespace {

/// @brief バインド先の同型コンポーネントをcomponentIndex番目（追加順）から探す（自分自身は除外）
IObjectComponent *FindParameterBindingTarget(ObjectContext *objectContext, const ParameterBinding &binding, const IObjectComponent *self) {
    if (!objectContext) return nullptr;
    int typeIndex = 0;
    for (const auto &componentPair : objectContext->GetAllComponents()) {
        IObjectComponent *candidate = componentPair.first.get();
        if (!candidate || candidate->GetComponentType() != binding.componentType) continue;
        if (typeIndex == binding.componentIndex) {
            return (candidate != self) ? candidate : nullptr;
        }
        ++typeIndex;
    }
    return nullptr;
}

/// @brief MyAnyが数値系（Bool/Int32/Float/Double）ならfloatへ変換する
bool TryAnyToFloat(const MyAny &value, float &outValue) {
    if (value.IsType<float>()) { outValue = value.AnyCast<float>(); return true; }
    if (value.IsType<double>()) { outValue = static_cast<float>(value.AnyCast<double>()); return true; }
    if (value.IsType<int>()) { outValue = static_cast<float>(value.AnyCast<int>()); return true; }
    if (value.IsType<bool>()) { outValue = value.AnyCast<bool>() ? 1.0f : 0.0f; return true; }
    return false;
}

/// @brief 値の型が数値系（Bool/Int32/Float/Double）かどうか
bool IsNumericValueType(ValueType type) {
    return type == ValueType::Bool || type == ValueType::Int32 || type == ValueType::Float || type == ValueType::Double;
}

} // namespace

bool ApplyParameterBinding(ObjectContext *objectContext, const ParameterBinding &binding, float value, const IObjectComponent *self) {
    IObjectComponent *target = FindParameterBindingTarget(objectContext, binding, self);
    if (!target) return false;

    // ScriptComponentの[SerializeField]変数への適用
    if (binding.isScriptVariable) {
        auto *script = dynamic_cast<ScriptComponent *>(target);
        return script && script->SetFloatVariable(binding.parameterName, value);
    }

    // メンバ変数（ADD_MEMBER_VARIABLE登録済み）への適用
    auto *member = target->GetMemberVariable(binding.parameterName);
    if (!member || !member->ptr) return false;
    bool written = false;
    switch (member->typeInfo.GetBaseType()) {
    case ValueType::Float:
        *static_cast<float *>(member->ptr) = value;
        written = true;
        break;
    case ValueType::Double:
        *static_cast<double *>(member->ptr) = static_cast<double>(value);
        written = true;
        break;
    case ValueType::Vector2:
    case ValueType::Vector3:
    case ValueType::Vector4: {
        const ValueType baseType = member->typeInfo.GetBaseType();
        const int channelCount = (baseType == ValueType::Vector2) ? 2 : (baseType == ValueType::Vector3) ? 3 : 4;
        const int channel = std::clamp(binding.channel, 0, channelCount - 1);
        static_cast<float *>(member->ptr)[channel] = value;
        written = true;
        break;
    }
    default:
        break;
    }
    // 書き込み後コールバック（Transformのワールド行列キャッシュ無効化など）
    if (written && member->onModified) {
        member->onModified();
    }
    return written;
}

bool ApplyParameterBindingValue(ObjectContext *objectContext, const ParameterBinding &binding, const MyAny &value, const IObjectComponent *self) {
    IObjectComponent *target = FindParameterBindingTarget(objectContext, binding, self);
    if (!target) return false;

    // ScriptComponentの[SerializeField]は現状float変数のみ対応のため、数値系のみ書き込める
    if (binding.isScriptVariable) {
        float floatValue = 0.0f;
        if (!TryAnyToFloat(value, floatValue)) return false;
        auto *script = dynamic_cast<ScriptComponent *>(target);
        return script && script->SetFloatVariable(binding.parameterName, floatValue);
    }

    auto *member = target->GetMemberVariable(binding.parameterName);
    if (!member || !member->ptr) return false;
    bool written = false;
    switch (member->typeInfo.GetBaseType()) {
    case ValueType::Float: {
        float v;
        if (TryAnyToFloat(value, v)) { *static_cast<float *>(member->ptr) = v; written = true; }
        break;
    }
    case ValueType::Double: {
        float v;
        if (TryAnyToFloat(value, v)) { *static_cast<double *>(member->ptr) = static_cast<double>(v); written = true; }
        break;
    }
    case ValueType::Bool:
        if (value.IsType<bool>()) {
            *static_cast<bool *>(member->ptr) = value.AnyCast<bool>();
            written = true;
        } else if (float v; TryAnyToFloat(value, v)) {
            *static_cast<bool *>(member->ptr) = (v != 0.0f);
            written = true;
        }
        break;
    case ValueType::Int32:
        if (value.IsType<int>()) {
            *static_cast<int *>(member->ptr) = value.AnyCast<int>();
            written = true;
        } else if (float v; TryAnyToFloat(value, v)) {
            *static_cast<int *>(member->ptr) = static_cast<int>(v);
            written = true;
        }
        break;
    case ValueType::Vector2:
    case ValueType::Vector3:
    case ValueType::Vector4: {
        const ValueType baseType = member->typeInfo.GetBaseType();
        if (binding.channel < 0) {
            // 値全体（Vector2/3/4）をそのまま書き込む。値の型が一致する場合のみ
            if (baseType == ValueType::Vector2 && value.IsType<Vector2>()) { *static_cast<Vector2 *>(member->ptr) = value.AnyCast<Vector2>(); written = true; }
            else if (baseType == ValueType::Vector3 && value.IsType<Vector3>()) { *static_cast<Vector3 *>(member->ptr) = value.AnyCast<Vector3>(); written = true; }
            else if (baseType == ValueType::Vector4 && value.IsType<Vector4>()) { *static_cast<Vector4 *>(member->ptr) = value.AnyCast<Vector4>(); written = true; }
        } else if (float v; TryAnyToFloat(value, v)) {
            const int channelCount = (baseType == ValueType::Vector2) ? 2 : (baseType == ValueType::Vector3) ? 3 : 4;
            const int channel = std::clamp(binding.channel, 0, channelCount - 1);
            static_cast<float *>(member->ptr)[channel] = v;
            written = true;
        }
        break;
    }
    case ValueType::String:
        if (value.IsType<std::string>()) {
            *static_cast<std::string *>(member->ptr) = value.AnyCast<std::string>();
            written = true;
        }
        break;
    case ValueType::Quaternion:
        if (value.IsType<Quaternion>()) {
            *static_cast<Quaternion *>(member->ptr) = value.AnyCast<Quaternion>();
            written = true;
        }
        break;
    case ValueType::Matrix4x4:
        if (value.IsType<Matrix4x4>()) {
            *static_cast<Matrix4x4 *>(member->ptr) = value.AnyCast<Matrix4x4>();
            written = true;
        }
        break;
    default:
        break;
    }
    if (written && member->onModified) {
        member->onModified();
    }
    return written;
}

JSON SaveParameterBindingToJson(const ParameterBinding &binding) {
    return JSON{
        { "componentType", binding.componentType },
        { "componentIndex", binding.componentIndex },
        { "parameterName", binding.parameterName },
        { "channel", binding.channel },
        { "isScriptVariable", binding.isScriptVariable },
    };
}

ParameterBinding LoadParameterBindingFromJson(const JSON &json) {
    ParameterBinding binding;
    if (!json.is_object()) return binding;
    binding.componentType = json.value("componentType", std::string{});
    binding.componentIndex = json.value("componentIndex", 0);
    binding.parameterName = json.value("parameterName", std::string{});
    binding.channel = json.value("channel", 0);
    binding.isScriptVariable = json.value("isScriptVariable", false);
    return binding;
}

#if defined(USE_IMGUI)

std::vector<ParameterBindingCandidate> CollectParameterBindingCandidates(ObjectContext *objectContext, const IObjectComponent *self) {
    std::vector<ParameterBindingCandidate> candidates;
    if (!objectContext) return candidates;

    std::unordered_map<std::string, int> typeCounts;
    for (const auto &componentPair : objectContext->GetAllComponents()) {
        IObjectComponent *component = componentPair.first.get();
        if (!component) continue;
        const std::string &type = component->GetComponentType();
        const int index = typeCounts[type]++;
        // 呼び出し元自身は適用先にできない（インデックスの整合のためカウントは進める）
        if (component == self) continue;
        const std::string prefix = type + "#" + std::to_string(index) + " / ";

        // ScriptComponentは[SerializeField]付きfloat変数を候補にする
        if (auto *script = dynamic_cast<ScriptComponent *>(component)) {
            for (const auto &variableName : script->GetFloatVariableNames()) {
                ParameterBindingCandidate candidate;
                candidate.binding.componentType = type;
                candidate.binding.componentIndex = index;
                candidate.binding.parameterName = variableName;
                candidate.binding.isScriptVariable = true;
                candidate.label = prefix + variableName + " (script)";
                candidates.push_back(std::move(candidate));
            }
            continue;
        }

        // 他コンポーネントはADD_MEMBER_VARIABLE登録済みのfloat系メンバ変数を候補にする
        for (const auto &[variableName, member] : component->GetAllMemberVariables()) {
            const ValueType baseType = member.typeInfo.GetBaseType();
            int channelCount = 0;
            if (baseType == ValueType::Float || baseType == ValueType::Double) channelCount = 1;
            else if (baseType == ValueType::Vector2) channelCount = 2;
            else if (baseType == ValueType::Vector3) channelCount = 3;
            else if (baseType == ValueType::Vector4) channelCount = 4;
            if (channelCount == 0) continue;

            static constexpr const char *kChannelNames[] = { "x", "y", "z", "w" };
            for (int channel = 0; channel < channelCount; ++channel) {
                ParameterBindingCandidate candidate;
                candidate.binding.componentType = type;
                candidate.binding.componentIndex = index;
                candidate.binding.parameterName = variableName;
                candidate.binding.channel = channel;
                candidate.label = prefix + variableName;
                if (channelCount > 1) {
                    candidate.label += std::string(".") + kChannelNames[channel];
                }
                candidates.push_back(std::move(candidate));
            }
        }
    }

    // unordered_mapの列挙順に依存しないよう、表示ラベルでソートして順序を安定させる
    std::sort(candidates.begin(), candidates.end(),
        [](const ParameterBindingCandidate &a, const ParameterBindingCandidate &b) { return a.label < b.label; });
    return candidates;
}

std::vector<ParameterBindingCandidate> CollectParameterBindingCandidatesForType(ObjectContext *objectContext, const IObjectComponent *self, const TypeInfo &sourceType) {
    std::vector<ParameterBindingCandidate> candidates;
    if (!objectContext) return candidates;
    const ValueType sourceBaseType = sourceType.GetBaseType();
    const bool sourceIsNumeric = IsNumericValueType(sourceBaseType);

    std::unordered_map<std::string, int> typeCounts;
    for (const auto &componentPair : objectContext->GetAllComponents()) {
        IObjectComponent *component = componentPair.first.get();
        if (!component) continue;
        const std::string &type = component->GetComponentType();
        const int index = typeCounts[type]++;
        if (component == self) continue;
        const std::string prefix = type + "#" + std::to_string(index) + " / ";

        // ScriptComponentの[SerializeField]付きfloat変数は数値系の値のみ候補にできる
        if (sourceIsNumeric) {
            if (auto *script = dynamic_cast<ScriptComponent *>(component)) {
                for (const auto &variableName : script->GetFloatVariableNames()) {
                    ParameterBindingCandidate candidate;
                    candidate.binding.componentType = type;
                    candidate.binding.componentIndex = index;
                    candidate.binding.parameterName = variableName;
                    candidate.binding.isScriptVariable = true;
                    candidate.label = prefix + variableName + " (script)";
                    candidates.push_back(std::move(candidate));
                }
                continue;
            }
        }

        for (const auto &[variableName, member] : component->GetAllMemberVariables()) {
            const ValueType baseType = member.typeInfo.GetBaseType();

            if (sourceIsNumeric) {
                // 数値系の値は、float/double/Vector2/3/4（成分単位）・Bool・Int32へ書き込める
                int channelCount = 0;
                if (baseType == ValueType::Float || baseType == ValueType::Double || baseType == ValueType::Bool || baseType == ValueType::Int32) channelCount = 1;
                else if (baseType == ValueType::Vector2) channelCount = 2;
                else if (baseType == ValueType::Vector3) channelCount = 3;
                else if (baseType == ValueType::Vector4) channelCount = 4;
                if (channelCount == 0) continue;

                static constexpr const char *kChannelNames[] = { "x", "y", "z", "w" };
                for (int channel = 0; channel < channelCount; ++channel) {
                    ParameterBindingCandidate candidate;
                    candidate.binding.componentType = type;
                    candidate.binding.componentIndex = index;
                    candidate.binding.parameterName = variableName;
                    candidate.binding.channel = channel;
                    candidate.label = prefix + variableName;
                    if (channelCount > 1) {
                        candidate.label += std::string(".") + kChannelNames[channel];
                    }
                    candidates.push_back(std::move(candidate));
                }
            } else if (baseType == sourceBaseType) {
                // Vector2/3/4・String・Quaternion・Matrix4x4は、同じ型のメンバへ値全体を書き込む（channel = -1）
                ParameterBindingCandidate candidate;
                candidate.binding.componentType = type;
                candidate.binding.componentIndex = index;
                candidate.binding.parameterName = variableName;
                candidate.binding.channel = -1;
                candidate.label = prefix + variableName;
                candidates.push_back(std::move(candidate));
            }
        }
    }

    std::sort(candidates.begin(), candidates.end(),
        [](const ParameterBindingCandidate &a, const ParameterBindingCandidate &b) { return a.label < b.label; });
    return candidates;
}

void ShowParameterBindingListImGui(std::vector<ParameterBinding> &bindings, const std::vector<ParameterBindingCandidate> &candidates) {
    ImGui::Text(TranslationC("component.parameterbinding.bindings_d"), static_cast<int>(bindings.size()));
    ImGui::SameLine();
    if (ImGui::SmallButton(TranslationLabel("component.parameterbinding.add_binding"))) {
        bindings.push_back(candidates.empty() ? ParameterBinding{} : candidates.front().binding);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "値の適用先を追加する。候補は同オブジェクトのコンポーネントのfloat系パラメータと\nScriptComponentの[SerializeField]付きfloat変数");
    }

    int removeBindingIndex = -1;
    for (int bindingIndex = 0; bindingIndex < static_cast<int>(bindings.size()); ++bindingIndex) {
        auto &binding = bindings[bindingIndex];
        ImGui::PushID(bindingIndex);

        // 現在のバインド内容に一致する候補を探す（コンボの選択表示用）
        int currentCandidate = -1;
        for (int c = 0; c < static_cast<int>(candidates.size()); ++c) {
            const auto &candidateBinding = candidates[c].binding;
            if (candidateBinding.componentType == binding.componentType &&
                candidateBinding.componentIndex == binding.componentIndex &&
                candidateBinding.parameterName == binding.parameterName &&
                candidateBinding.channel == binding.channel &&
                candidateBinding.isScriptVariable == binding.isScriptVariable) {
                currentCandidate = c;
                break;
            }
        }
        std::string preview;
        if (currentCandidate >= 0) {
            preview = candidates[currentCandidate].label;
        } else if (binding.componentType.empty()) {
            preview = "(未設定)";
        } else {
            // 保存済みのバインド先が現在のコンポーネント構成に見つからない場合
            preview = binding.componentType + "#" + std::to_string(binding.componentIndex) + " / " + binding.parameterName + " (不明)";
        }

        ImGui::SetNextItemWidth(280.0f);
        if (ImGui::BeginCombo("##bindingTarget", preview.c_str())) {
            for (int c = 0; c < static_cast<int>(candidates.size()); ++c) {
                const bool selected = (c == currentCandidate);
                if (ImGui::Selectable(candidates[c].label.c_str(), selected)) {
                    binding = candidates[c].binding;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            removeBindingIndex = bindingIndex;
        }
        ImGui::PopID();
    }
    if (removeBindingIndex >= 0) {
        bindings.erase(bindings.begin() + removeBindingIndex);
    }
}

#endif // USE_IMGUI

} // namespace KashipanEngine
