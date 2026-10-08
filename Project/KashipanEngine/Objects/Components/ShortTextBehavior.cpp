#include "Objects/Components/ShortTextBehavior.h"
#include "Objects/Components/Transform.h"
#include "Scene/SceneContext.h"
#include "Utilities/TimeUtils.h"
#include "Utilities/ShortTextRecipeJson.h"
#include "Objects/Components/Velocity.h"
#include "Input/Input.h"
#include "Input/Keyboard.h"

namespace KashipanEngine {
namespace {
JSON RecipeJson(const ShortText::Recipe &r, const std::string &source) {
    JSON result = ShortText::EncodeRecipe(r);
    result["sourceText"] = source;
    return result;
}
}
JSON MakeShortTextBehaviorState(const ShortText::Recipe &r, const std::string &source) {
    return JSON{{"priority", r.kind == ShortText::Kind::Rules ? 0 : 1}, {"isActive", true}, {"tag", ""}, {"customData", RecipeJson(r, source)}};
}
std::unique_ptr<IObjectComponent> ShortTextBehavior::Clone() const {
    auto copy = std::make_unique<ShortTextBehavior>();
    copy->recipe_ = recipe_;
    copy->sourceText_ = sourceText_;
    return copy;
}
void ShortTextBehavior::Update() {
    auto *scene = GetOwnerSceneContext();
    if (!scene || !scene->IsPlaying()) return;
    auto *object = GetOwnerObjectContext();
    auto *transform = object ? object->GetComponent<Transform>() : nullptr;
    const double dt = GetDeltaTime();
    if (!transform || !std::isfinite(dt) || dt < 0) return;
    if (recipe_.kind == ShortText::Kind::Rules) {
        auto *velocity = object->GetComponent<Velocity>();
        if (ShortText::NeedsVelocity(recipe_.rules) && !velocity) return;
        ShortText::ProgramState state;
        auto read = [](const Vector3 &v) { return std::array<float, 3>{v.x, v.y, v.z}; };
        auto write = [](const std::array<float, 3> &v) { return Vector3(v[0], v[1], v[2]); };
        state.position = read(transform->GetTranslate());
        state.rotation = read(transform->GetRotate());
        state.scale = read(transform->GetScale());
        if (velocity) { state.velocity = read(velocity->GetVelocity()); state.acceleration = read(velocity->GetAcceleration()); }
        ShortText::InputSnapshot input;
        if (auto *device = scene->GetInput()) {
            const auto &keyboard = device->GetKeyboard();
            for (const auto &rule : recipe_.rules) {
                const auto key = static_cast<size_t>(rule.key);
                input.pressed[key] = keyboard.IsTrigger(rule.key);
                input.held[key] = keyboard.IsDown(rule.key);
                input.released[key] = keyboard.IsRelease(rule.key);
            }
        }
        ShortText::TickProgram(recipe_.rules, programMemory_, state, input, dt);
        transform->SetTranslate(write(state.position));
        transform->SetRotate(write(state.rotation));
        transform->SetScale(write(state.scale));
        if (velocity) { velocity->SetVelocity(write(state.velocity)); velocity->SetAcceleration(write(state.acceleration)); }
        return;
    }
    const auto delta = ShortText::EvaluateDelta(recipe_, elapsed_, dt);
    transform->SetTranslate(transform->GetTranslate() + Vector3(delta.translation[0], delta.translation[1], delta.translation[2]));
    transform->SetRotate(transform->GetRotate() + Vector3(delta.rotation[0], delta.rotation[1], delta.rotation[2]));
    if (recipe_.kind == ShortText::Kind::Oscillate) elapsed_ = std::fmod(elapsed_ + dt, recipe_.period);
}
JSON ShortTextBehavior::SaveToJson() const { return RecipeJson(recipe_, sourceText_); }
bool ShortTextBehavior::LoadFromJson(const JSON &json) {
    if (!json.is_object()) return false;
    try {
        auto next = ShortText::DecodeRecipe(json);
        const auto source = json.value("sourceText", std::string{});
        if (!ShortText::IsValid(next) || source.size() > 512) return false;
        recipe_ = next;
        sourceText_ = source;
        elapsed_ = 0;
        programMemory_ = {};
        return true;
    } catch (const std::exception &) { return false; }
}
#if defined(USE_IMGUI)
void ShortTextBehavior::ShowImGui() {
    ImGui::TextWrapped("%s", sourceText_.c_str());
    if (recipe_.kind == ShortText::Kind::Rules) {
        ImGui::TextWrapped("%s", ShortText::Describe(recipe_).c_str());
        if (ShortText::NeedsVelocity(recipe_.rules) && !GetOwnerObjectContext()->GetComponent<Velocity>())
            ImGui::TextWrapped("Velocityが必要です。短文の挙動から再適用すると追加されます。");
        for (const auto &rule : recipe_.rules) {
            ImGui::Text("%s / %s", ShortText::EventLabels[static_cast<size_t>(rule.event)], ShortText::KeyName(rule.key).c_str());
            for (const auto &action : rule.actions)
                ImGui::BulletText("%s %c = %.3f", ShortText::OperationLabels[static_cast<size_t>(action.operation)], "XYZ"[action.axis], action.value);
        }
        return;
    }
    const char *kinds[] = {"往復", "回転", "移動"};
    ImGui::Text("%s / %c軸", kinds[static_cast<int>(recipe_.kind)], "XYZ"[recipe_.axis]);
    ImGui::TextUnformatted("再生中に動作します。短文の挙動ウィンドウから置き換えられます。");
    bool changed = ImGui::Combo("軸", &recipe_.axis, "X\0Y\0Z\0");
    if (recipe_.kind == ShortText::Kind::Oscillate) {
        changed |= ImGui::DragFloat("振幅", &recipe_.amplitude, 0.01f, 0.001f, 10000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        changed |= ImGui::DragFloat("周期 (秒)", &recipe_.period, 0.01f, 0.05f, 3600.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    } else {
        const char *label = recipe_.kind == ShortText::Kind::Rotate ? "速度 (度/秒)" : "速度 (単位/秒)";
        changed |= ImGui::DragFloat(label, &recipe_.speed, 0.1f, 0.001f, 10000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        bool reverse = recipe_.direction < 0;
        if (ImGui::Checkbox("逆方向", &reverse)) { recipe_.direction = reverse ? -1 : 1; changed = true; }
    }
    if (changed) elapsed_ = 0;
}
#endif
} // namespace KashipanEngine
