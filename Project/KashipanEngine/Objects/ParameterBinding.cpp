#include "Objects/ParameterBinding.h"

#include <algorithm>
#include <cmath>
#include <limits>
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
        IObjectComponent *candidate = componentPair.first;
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

    const auto *member = target->GetMemberVariable(binding.parameterName);
    if (!member || !member->IsWritable()) return false;
    const ValueType type = member->GetTypeInfo().GetBaseType();
    if (type != ValueType::Float && type != ValueType::Double && type != ValueType::Vector2 &&
        type != ValueType::Vector3 && type != ValueType::Vector4) return false;
    // float専用APIは従来どおり成分を書き込む（値全体の指定はValue版のみ）。
    ParameterBinding channelBinding = binding;
    channelBinding.channel = std::max(0, binding.channel);
    return ApplyParameterBindingValue(objectContext, channelBinding, MyAny(value), self);
}

namespace {
template <typename T>
bool SetVectorChannel(IObjectComponent *target, const std::string &key, int channel, float value) {
    T vector;
    if (!target->GetMemberValue(key, vector)) return false;
    if (channel == 0) vector.x = value;
    else if (channel == 1) vector.y = value;
    else if constexpr (!std::is_same_v<T, Vector2>) {
        if (channel == 2) vector.z = value;
        else if constexpr (std::is_same_v<T, Vector4>) vector.w = value;
        else return false;
    } else return false;
    return target->SetMemberValue(key, vector);
}
template <typename T>
bool SetExactMemberValue(IObjectComponent *target, const std::string &key, const MyAny &value) {
    return value.IsType<T>() && target->SetMemberValue(key, value.AnyCast<T>());
}
} // namespace

bool ApplyParameterBindingValue(ObjectContext *objectContext, const ParameterBinding &binding, const MyAny &value, const IObjectComponent *self) {
    IObjectComponent *target = FindParameterBindingTarget(objectContext, binding, self);
    if (!target) return false;
    if (binding.isScriptVariable) {
        float v;
        if (!TryAnyToFloat(value, v)) return false;
        auto *script = dynamic_cast<ScriptComponent *>(target);
        return script && script->SetFloatVariable(binding.parameterName, v);
    }
    const auto *member = target->GetMemberVariable(binding.parameterName);
    if (!member || !member->IsWritable()) return false;
    const auto &key = binding.parameterName;
    float v = 0.0f;
    switch (member->GetTypeInfo().GetBaseType()) {
    case ValueType::Float:
        return TryAnyToFloat(value, v) && target->SetMemberValue(key, v);
    case ValueType::Double:
        if (value.IsType<double>()) return target->SetMemberValue(key, value.AnyCast<double>());
        return TryAnyToFloat(value, v) && target->SetMemberValue(key, static_cast<double>(v));
    case ValueType::Bool:
        return TryAnyToFloat(value, v) && target->SetMemberValue(key, v != 0.0f);
    case ValueType::Int32:
        if (value.IsType<int>()) return target->SetMemberValue(key, value.AnyCast<int>());
        // 浮動小数点から整数への範囲外変換を拒否する。
        if (!TryAnyToFloat(value, v) || !std::isfinite(v) ||
            static_cast<double>(v) < std::numeric_limits<int>::min() ||
            static_cast<double>(v) > std::numeric_limits<int>::max()) return false;
        return target->SetMemberValue(key, static_cast<int>(v));
    case ValueType::Vector2:
        if (binding.channel < 0) return SetExactMemberValue<Vector2>(target, key, value);
        return TryAnyToFloat(value, v) && SetVectorChannel<Vector2>(target, key, std::clamp(binding.channel, 0, 1), v);
    case ValueType::Vector3:
        if (binding.channel < 0) return SetExactMemberValue<Vector3>(target, key, value);
        return TryAnyToFloat(value, v) && SetVectorChannel<Vector3>(target, key, std::clamp(binding.channel, 0, 2), v);
    case ValueType::Vector4:
        if (binding.channel < 0) return SetExactMemberValue<Vector4>(target, key, value);
        return TryAnyToFloat(value, v) && SetVectorChannel<Vector4>(target, key, std::clamp(binding.channel, 0, 3), v);
    case ValueType::String: return SetExactMemberValue<std::string>(target, key, value);
    case ValueType::Quaternion: return SetExactMemberValue<Quaternion>(target, key, value);
    case ValueType::Matrix4x4: return SetExactMemberValue<Matrix4x4>(target, key, value);
    default: return false;
    }
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
        IObjectComponent *component = componentPair.first;
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
            if (!member.IsWritable()) continue;
            const ValueType baseType = member.GetTypeInfo().GetBaseType();
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
        IObjectComponent *component = componentPair.first;
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
            if (!member.IsWritable()) continue;
            const ValueType baseType = member.GetTypeInfo().GetBaseType();

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
        ImGuiCustom::SetTooltipWrapped("%s", "値の適用先を追加する。候補は同オブジェクトのコンポーネントのfloat系パラメータと\nScriptComponentの[SerializeField]付きfloat変数");
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
