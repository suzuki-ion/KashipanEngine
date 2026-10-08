#include "Objects/Components/ShortTextBehavior.h"
#include "Objects/Components/Transform.h"
#include "Scene/SceneContext.h"
#include "Utilities/TimeUtils.h"

namespace KashipanEngine {
namespace {
JSON RecipeJson(const ShortText::Recipe &r, const std::string &source) {
    return JSON{{"kind", static_cast<int>(r.kind)}, {"axis", r.axis}, {"direction", r.direction},
        {"amplitude", r.amplitude}, {"period", r.period}, {"speed", r.speed}, {"sourceText", source}};
}
}
JSON MakeShortTextBehaviorState(const ShortText::Recipe &r, const std::string &source) {
    return JSON{{"priority", 1}, {"isActive", true}, {"tag", ""}, {"customData", RecipeJson(r, source)}};
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
    if (!transform || !std::isfinite(dt) || dt <= 0) return;
    const auto delta = ShortText::EvaluateDelta(recipe_, elapsed_, dt);
    transform->SetTranslate(transform->GetTranslate() + Vector3(delta.translation[0], delta.translation[1], delta.translation[2]));
    transform->SetRotate(transform->GetRotate() + Vector3(delta.rotation[0], delta.rotation[1], delta.rotation[2]));
    if (recipe_.kind == ShortText::Kind::Oscillate) elapsed_ = std::fmod(elapsed_ + dt, recipe_.period);
}
JSON ShortTextBehavior::SaveToJson() const { return RecipeJson(recipe_, sourceText_); }
bool ShortTextBehavior::LoadFromJson(const JSON &json) {
    if (!json.is_object()) return false;
    try {
        ShortText::Recipe next;
        auto readInt = [&](const char *name, int defaultValue, int min, int max, int &out) {
            const auto it = json.find(name);
            if (it == json.end()) { out = defaultValue; return true; }
            if (!it->is_number_integer()) return false;
            if (it->is_number_unsigned()) {
                if (it->get<std::uint64_t>() > static_cast<std::uint64_t>(max)) return false;
            } else {
                const auto value = it->get<std::int64_t>();
                if (value < min || value > max) return false;
            }
            out = it->get<int>();
            return true;
        };
        int kind = 0;
        if (!readInt("kind", 0, 0, 2, kind) || !readInt("axis", 1, 0, 2, next.axis) ||
            !readInt("direction", 1, -1, 1, next.direction)) return false;
        next.kind = static_cast<ShortText::Kind>(kind);
        next.amplitude = json.value("amplitude", 0.5f);
        next.period = json.value("period", 2.0f);
        next.speed = json.value("speed", 1.0f);
        const auto source = json.value("sourceText", std::string{});
        if (!ShortText::IsValid(next) || source.size() > 512) return false;
        recipe_ = next;
        sourceText_ = source;
        elapsed_ = 0;
        return true;
    } catch (const JSON::exception &) { return false; }
}
#if defined(USE_IMGUI)
void ShortTextBehavior::ShowImGui() {
    ImGui::TextWrapped("%s", sourceText_.c_str());
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
