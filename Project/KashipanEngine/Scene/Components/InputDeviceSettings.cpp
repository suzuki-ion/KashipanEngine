#include "Scene/Components/InputDeviceSettings.h"

#include <algorithm>

#include "EngineSettings.h"
#include "Input/Input.h"

namespace KashipanEngine {
namespace {
JSON OverrideToJson(bool enabled, float value) {
    JSON json;
    json["override"] = enabled;
    json["value"] = value;
    return json;
}
} // namespace

void InputDeviceSettings::LoadDefaultValues() {
    const auto &settings = GetEngineSettings().input;
    mouseMoveThreshold_.value = settings.mouseMoveThreshold;
    controllerStickThreshold_.value = settings.controllerStickThreshold;
    controllerTriggerThreshold_.value = settings.controllerTriggerThreshold;
}

void InputDeviceSettings::Apply() const {
    SceneContext *sceneContext = GetOwnerSceneContext();
    Input *input = sceneContext ? sceneContext->GetInput() : nullptr;
    if (!input) return;

    input->ResetDeviceThresholds();
    if (mouseMoveThreshold_.enabled) input->SetMouseMoveThreshold(mouseMoveThreshold_.value);
    if (controllerStickThreshold_.enabled) input->SetControllerStickThreshold(controllerStickThreshold_.value);
    if (controllerTriggerThreshold_.enabled) input->SetControllerTriggerThreshold(controllerTriggerThreshold_.value);
}

void InputDeviceSettings::Finalize() {
    // エディターでの削除・無効化時に上書きを取り消す
    SceneContext *sceneContext = GetOwnerSceneContext();
    Input *input = sceneContext ? sceneContext->GetInput() : nullptr;
    if (input) input->ResetDeviceThresholds();
}

#if defined(USE_IMGUI)
void InputDeviceSettings::ShowImGui() {
    const auto &defaults = GetEngineSettings().input;
    ImGuiCustom::TextDisabledWrapped("%s", TranslationC("component.inputdevicesettings.desc"));

    bool changed = false;
    const auto overrideRow = [&](const char *id, OverrideValue &target, float defaultValue, auto &&drawValue) {
        ImGui::PushID(id);
        changed |= ImGui::Checkbox("##override", &target.enabled);
        if (ImGui::IsItemHovered()) {
            ImGuiCustom::SetTooltipWrapped("%s", TranslationC("component.inputdevicesettings.override"));
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!target.enabled);
        changed |= drawValue(target.value);
        ImGui::EndDisabled();
        if (!target.enabled) {
            ImGuiCustom::TextDisabledWrapped(TranslationC("component.inputdevicesettings.default_f"), defaultValue);
        }
        ImGui::PopID();
    };

    overrideRow("MouseMove", mouseMoveThreshold_, defaults.mouseMoveThreshold, [](float &value) {
        const bool edited = ImGui::DragFloat(TranslationLabel("component.inputdevicesettings.mouse_move_threshold"), &value, 0.5f, 0.0f, 500.0f, "%.1f");
        value = (std::max)(value, 0.0f);
        return edited;
    });
    overrideRow("Stick", controllerStickThreshold_, defaults.controllerStickThreshold, [](float &value) {
        return ImGui::SliderFloat(TranslationLabel("component.inputdevicesettings.stick_threshold"), &value, 0.0f, 1.0f, "%.2f");
    });
    overrideRow("Trigger", controllerTriggerThreshold_, defaults.controllerTriggerThreshold, [](float &value) {
        return ImGui::SliderFloat(TranslationLabel("component.inputdevicesettings.trigger_threshold"), &value, 0.0f, 1.0f, "%.2f");
    });

    if (changed && IsActive()) {
        Apply();
    }
}
#endif

JSON InputDeviceSettings::SaveToJson() const {
    JSON json;
    json["mouseMoveThreshold"] = OverrideToJson(mouseMoveThreshold_.enabled, mouseMoveThreshold_.value);
    json["controllerStickThreshold"] = OverrideToJson(controllerStickThreshold_.enabled, controllerStickThreshold_.value);
    json["controllerTriggerThreshold"] = OverrideToJson(controllerTriggerThreshold_.enabled, controllerTriggerThreshold_.value);
    return json;
}

bool InputDeviceSettings::LoadFromJson(const JSON &json) {
    const auto loadOverride = [&json](const char *key, OverrideValue &target) {
        if (!json.contains(key) || !json[key].is_object()) return;
        const JSON &entry = json[key];
        target.enabled = entry.value("override", false);
        target.value = entry.value("value", target.value);
    };
    loadOverride("mouseMoveThreshold", mouseMoveThreshold_);
    loadOverride("controllerStickThreshold", controllerStickThreshold_);
    loadOverride("controllerTriggerThreshold", controllerTriggerThreshold_);

    mouseMoveThreshold_.value = (std::max)(mouseMoveThreshold_.value, 0.0f);
    controllerStickThreshold_.value = std::clamp(controllerStickThreshold_.value, 0.0f, 1.0f);
    controllerTriggerThreshold_.value = std::clamp(controllerTriggerThreshold_.value, 0.0f, 1.0f);

    // シーン読み込みではAddComponent（Initialize）の後にLoadFromJsonが呼ばれるため、読み込んだ値をここで反映する
    if (GetOwnerSceneContext() && IsActive()) {
        Apply();
    }
    return true;
}

} // namespace KashipanEngine
