#include "Objects/Components/PropertyMotion.h"
#include "Objects/Components/ShortTextExtended.h"
#include "Objects/EmptyObject.h"
#include "Scene/SceneContext.h"
#include "Utilities/TimeUtils.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Math/Color.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace KashipanEngine {
namespace {
constexpr double kDegreesToRadians = 3.14159265358979323846 / 180.0;
const char *UnitName(ShortText::PropertyUnit unit) { return unit == ShortText::PropertyUnit::Degrees ? "degrees" : "raw"; }
const char *WaveName(ShortText::PropertyWave wave) { return wave == ShortText::PropertyWave::Smooth ? "smooth" : "linear"; }
bool IsTypeName(const std::string &text) {
    if (text.empty() || text.size() > 64) return false;
    for (char c : text) if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) return false;
    return true;
}
}

ShortText::PropertyMemberInfo InspectPropertyMember(const IObjectComponent *component, const std::string &member) {
    ShortText::PropertyMemberInfo info;
    if (!component) return info;
    const auto *variable = component->GetMemberVariable(member);
    if (!variable) return info;
    info.exists = true;
    info.writable = variable->IsWritable();
    float f; double d; Vector2 v2; Vector3 v3; Vector4 v4; Color color;
    if (component->GetMemberValue(member, f) || component->GetMemberValue(member, d)) info.components = 1;
    else if (component->GetMemberValue(member, v2)) info.components = 2;
    else if (component->GetMemberValue(member, v3)) info.components = 3;
    else if (component->GetMemberValue(member, v4) || component->GetMemberValue(member, color)) info.components = 4;
    return info;
}

IObjectComponent *FindComponentOfType(const EmptyObject *object, const std::string &type, int index) {
    if (!object || index < 0) return nullptr;
    for (const auto &[component, addedID] : object->GetAllComponents()) {
        (void)addedID;
        if (component && component->GetComponentType() == type && index-- == 0) return component;
    }
    return nullptr;
}

JSON EncodePropertyMotionSettings(const PropertyMotion::Settings &s) {
    return JSON{{"component", s.component}, {"componentIndex", s.componentIndex}, {"member", s.member}, {"axis", s.axis},
        {"from", s.from}, {"to", s.to}, {"period", s.period}, {"unit", UnitName(s.unit)}, {"wave", WaveName(s.wave)},
        {"shortTextManaged", s.shortTextManaged}, {"sourceText", s.sourceText}};
}

bool DecodePropertyMotionSettings(const JSON &json, PropertyMotion::Settings &out) {
    if (!json.is_object()) return false;
    PropertyMotion::Settings s;
    auto text = [&](const char *key, std::string &value) {
        if (!json.contains(key)) return true;
        if (!json[key].is_string()) return false;
        value = json[key].get<std::string>(); return true;
    };
    auto integer = [&](const char *key, int &value, int low, int high) {
        if (!json.contains(key)) return true;
        const auto &v = json[key];
        if (!v.is_number_integer()) return false;
        const auto n = v.get<long long>();
        if (n < low || n > high) return false;
        value = static_cast<int>(n); return true;
    };
    auto number = [&](const char *key, float &value) {
        if (!json.contains(key)) return true;
        if (!json[key].is_number()) return false;
        const double n = json[key].get<double>();
        if (!std::isfinite(n) || std::abs(n) > ShortText::PropertyValueLimit) return false;
        value = static_cast<float>(n); return true;
    };
    std::string unit = UnitName(s.unit), wave = WaveName(s.wave);
    if (!text("component", s.component) || !integer("componentIndex", s.componentIndex, 0, 255) || !text("member", s.member) ||
        !integer("axis", s.axis, -1, 3) || !number("from", s.from) || !number("to", s.to) || !number("period", s.period) ||
        !text("unit", unit) || !text("wave", wave) || !text("sourceText", s.sourceText)) return false;
    if (json.contains("shortTextManaged")) {
        if (!json["shortTextManaged"].is_boolean()) return false;
        s.shortTextManaged = json["shortTextManaged"].get<bool>();
    }
    if (unit != "raw" && unit != "degrees") return false;
    if (wave != "linear" && wave != "smooth") return false;
    s.unit = unit == "degrees" ? ShortText::PropertyUnit::Degrees : ShortText::PropertyUnit::Raw;
    s.wave = wave == "smooth" ? ShortText::PropertyWave::Smooth : ShortText::PropertyWave::Linear;
    if (!PropertyMotion::IsValid(s)) return false;
    out = s;
    return true;
}

JSON MakePropertyMotionState(const PropertyMotion::Settings &settings) {
    return JSON{{"priority", 1}, {"isActive", true}, {"tag", ""}, {"customData", EncodePropertyMotionSettings(settings)}};
}

bool PropertyMotion::IsValid(const Settings &s) {
    auto finite = [](float v) { return std::isfinite(v) && std::abs(v) <= ShortText::PropertyValueLimit; };
    if (!IsTypeName(s.component) || s.component == "PropertyMotion" || !IsTypeName(s.member)) return false;
    if (s.componentIndex < 0 || s.componentIndex > 255 || s.axis < -1 || s.axis > 3) return false;
    if (!finite(s.from) || !finite(s.to) || !(s.from < s.to)) return false;
    if (!std::isfinite(s.period) || s.period < 0.05f || s.period > 3600) return false;
    if (ShortText::IsPositiveMember(s.component, s.member) && s.from <= 0) return false;
    return s.sourceText.size() <= 512;
}

bool PropertyMotion::SetSettings(const Settings &settings) {
    if (!IsValid(settings)) return false;
    settings_ = settings;
    elapsed_ = 0;
    lastError_.clear();
    return true;
}

std::unique_ptr<IObjectComponent> PropertyMotion::Clone() const {
    auto copy = std::make_unique<PropertyMotion>();
    copy->settings_ = settings_;
    return copy;
}

std::string PropertyMotion::GetTargetPath() const {
    std::string path = settings_.component;
    if (settings_.componentIndex > 0) path += "[" + std::to_string(settings_.componentIndex) + "]";
    path += "." + settings_.member;
    if (settings_.axis >= 0) path += std::string(".") + "xyzw"[settings_.axis];
    return path;
}

float PropertyMotion::GetCurrentValue() const {
    return ShortText::EvaluatePropertyPingPong(settings_.from, settings_.to, settings_.period, elapsed_, settings_.wave);
}

bool PropertyMotion::Advance(double dt) {
    if (!std::isfinite(dt) || dt < 0) return false;
    elapsed_ = std::fmod(elapsed_ + dt, static_cast<double>(settings_.period));
    auto *context = GetOwnerObjectContext();
    auto *owner = context ? const_cast<EmptyObject *>(context->GetOwner()) : nullptr;
    auto *component = FindComponentOfType(owner, settings_.component, settings_.componentIndex);
    if (!component) { lastError_ = settings_.component + " が見つかりません。"; return false; }
    const auto info = InspectPropertyMember(component, settings_.member);
    if (!info.exists || !info.writable || info.components < 1) { lastError_ = settings_.member + " は書き込める数値の項目ではありません。"; return false; }
    if ((info.components == 1) != (settings_.axis < 0) || settings_.axis >= info.components) { lastError_ = "成分の指定が項目の型と一致しません。"; return false; }
    double value = GetCurrentValue();
    if (settings_.unit == ShortText::PropertyUnit::Degrees) value *= kDegreesToRadians;
    if (!ShortText::WriteExtendedProperty(owner, GetTargetPath(), ShortText::Value(value))) { lastError_ = GetTargetPath() + " へ書き込めませんでした。"; return false; }
    lastError_.clear();
    return true;
}

void PropertyMotion::Initialize() {
    elapsed_ = 0;
    playing_ = false;
    lastError_.clear();
}

void PropertyMotion::Update() {
    auto *scene = GetOwnerSceneContext();
    if (!scene || !scene->IsPlaying()) { playing_ = false; return; } // 編集中の値は動かさない
    if (!playing_) {
        // 再生開始フレームは必ず開始値から始める
        playing_ = true;
        elapsed_ = 0;
        Advance(0);
        return;
    }
    Advance(GetDeltaTime());
}

JSON PropertyMotion::SaveToJson() const { return EncodePropertyMotionSettings(settings_); }

bool PropertyMotion::LoadFromJson(const JSON &json) {
    Settings next;
    if (!DecodePropertyMotionSettings(json, next)) return false;
    settings_ = next;
    elapsed_ = 0;
    lastError_.clear();
    return true;
}

#if defined(USE_IMGUI)
void PropertyMotion::ShowImGui() {
    auto next = settings_;
    auto *context = GetOwnerObjectContext();
    const EmptyObject *owner = context ? context->GetOwner() : nullptr;
    if (next.shortTextManaged) ImGui::TextWrapped("短文から作成: %s", next.sourceText.c_str());
    ImGui::TextWrapped("再生中だけ、開始値→終了値→開始値の順に往復します（ローカル絶対値）。編集中の値は変わらず、停止すると再生前に戻ります。");
    // 対象コンポーネント（自分以外の型）
    std::vector<std::string> types;
    if (owner) for (const auto &[component, addedID] : owner->GetAllComponents()) {
        (void)addedID;
        if (!component || component == this) continue;
        const auto &type = component->GetComponentType();
        if (type != "PropertyMotion" && std::find(types.begin(), types.end(), type) == types.end()) types.push_back(type);
    }
    if (ImGui::BeginCombo("対象コンポーネント", next.component.c_str())) {
        for (const auto &type : types) if (ImGui::Selectable(type.c_str(), type == next.component)) { next.component = type; next.componentIndex = 0; }
        ImGui::EndCombo();
    }
    ImGui::InputInt("同じ型の番号", &next.componentIndex);
    next.componentIndex = std::clamp(next.componentIndex, 0, 255);
    const auto *target = FindComponentOfType(owner, next.component, next.componentIndex);
    if (!target) ImGui::TextWrapped("%s がありません。追加するか、対象を選び直してください。", next.component.c_str());
    if (ImGui::BeginCombo("プロパティ", next.member.c_str())) {
        if (target) {
            std::vector<std::string> names;
            for (const auto &[name, variable] : target->GetAllMemberVariables()) {
                (void)variable;
                const auto info = InspectPropertyMember(target, name);
                if (info.writable && info.components > 0) names.push_back(name);
            }
            std::sort(names.begin(), names.end());
            for (const auto &name : names) if (ImGui::Selectable(name.c_str(), name == next.member)) {
                next.member = name;
                const int components = InspectPropertyMember(target, name).components;
                next.axis = components == 1 ? -1 : std::clamp(next.axis, 0, components - 1);
                next.unit = ShortText::IsAngleMember(next.component, name) ? ShortText::PropertyUnit::Degrees : ShortText::PropertyUnit::Raw;
            }
        }
        ImGui::EndCombo();
    }
    const int components = target ? InspectPropertyMember(target, next.member).components : 3;
    if (components > 1) {
        int axis = std::max(next.axis, 0);
        const char *axes[] = {"X", "Y", "Z", "W"};
        if (ImGui::BeginCombo("成分", axes[std::min(axis, 3)])) {
            for (int i = 0; i < components && i < 4; ++i) if (ImGui::Selectable(axes[i], i == axis)) axis = i;
            ImGui::EndCombo();
        }
        next.axis = axis;
    } else next.axis = -1;
    int unit = static_cast<int>(next.unit);
    if (ImGui::Combo("単位", &unit, "そのまま書き込む\0度（ラジアンへ変換）\0")) next.unit = static_cast<ShortText::PropertyUnit>(unit);
    const float low = ShortText::IsPositiveMember(next.component, next.member) ? 0.001f : -ShortText::PropertyValueLimit;
    ImGui::DragFloat("開始値", &next.from, 0.01f, low, next.to - 0.001f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("終了値", &next.to, 0.01f, next.from + 0.001f, ShortText::PropertyValueLimit, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("周期 (1往復の秒数)", &next.period, 0.01f, 0.05f, 3600.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    int wave = static_cast<int>(next.wave);
    if (ImGui::Combo("方式", &wave, "一定速度（三角波）\0なめらか（サイン）\0")) next.wave = static_cast<ShortText::PropertyWave>(wave);
    const bool sameSettings = EncodePropertyMotionSettings(next) == EncodePropertyMotionSettings(settings_);
    if (!sameSettings && !SetSettings(next)) ImGui::TextWrapped("設定が不正なため反映していません（開始値 < 終了値、サイズ等は正の値、周期0.05〜3600秒）。");
    ImGui::Text("書き込み先: %s", GetTargetPath().c_str());
    auto *scene = GetOwnerSceneContext();
    if (scene && scene->IsPlaying()) ImGui::Text("現在値: %.3f%s", GetCurrentValue(), settings_.unit == ShortText::PropertyUnit::Degrees ? " 度" : "");
    if (!lastError_.empty()) ImGui::TextWrapped("実行できませんでした: %s", lastError_.c_str());
}
#endif
} // namespace KashipanEngine
