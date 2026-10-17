#pragma once
// Private helpers shared by the gameplay component implementations (not a public API).
#include "Objects/Components/GameplayComponents.h"
#include <cmath>
#include "Objects/EmptyObject.h"
#if defined(USE_IMGUI)
#include <imgui.h>
#include <imgui_stdlib.h>
#endif

namespace KashipanEngine::GameplayDetail {
inline bool Fail(std::string *error, const std::string &message) { if (error) *error = message; return false; }
inline bool ValidDt(double dt) { return std::isfinite(dt) && dt >= 0 && dt <= 10; }
inline bool ValidEventName(const std::string &name) { return name.empty() || GameEvents::IsValidName(name); }
inline bool ValidTag(const std::string &tag) { return tag.size() <= 64 && tag.find_first_of("\r\n\t") == std::string::npos; }
inline EmptyObject *Owner(const IObjectComponent *component) { return const_cast<EmptyObject *>(component->GetOwnerObject()); }
template <class C> JSON SaveSettings(const typename C::Settings &settings) {
    JSON json = JSON::object();
    auto copy = settings;
    Gameplay::Fields fields(json);
    C::Bind(copy, fields);
    return json;
}
template <class C> bool LoadSettings(const JSON &json, typename C::Settings &out) {
    if (!json.is_object()) return false;
    typename C::Settings next;
    Gameplay::Fields fields(json);
    if (!C::Bind(next, fields) || !C::IsValid(next)) return false;
    out = std::move(next);
    return true;
}
inline bool ArgNumber(const GameEvents::Value &args, const char *key, double &out, std::string &error) {
    if (!args.is_object() || !args.contains(key) || !args[key].is_number()) { error = std::string("数値の引数 ") + key + " が必要です。"; return false; }
    out = args[key].get<double>();
    if (!std::isfinite(out)) { error = std::string(key) + " が有限の数値ではありません。"; return false; }
    return true;
}
inline bool ArgText(const GameEvents::Value &args, const char *key, std::string &out, std::string &error) {
    if (!args.is_object() || !args.contains(key) || !args[key].is_string()) { error = std::string("文字列の引数 ") + key + " が必要です。"; return false; }
    out = args[key].get<std::string>();
    return true;
}
#if defined(USE_IMGUI)
inline void ManagedHeader(bool managed, const std::string &source) {
    if (managed) ImGui::TextWrapped("短文から作成: %s", source.c_str());
}
inline void EventInput(const char *label, std::string &value) {
    ImGui::InputText(label, &value);
    if (!ValidEventName(value)) ImGui::TextWrapped("イベント名が不正です（空欄は送信しない）。");
}
template <class C> void ApplyEdit(C &component, const typename C::Settings &next, const typename C::Settings &current) {
    if (SaveSettings<C>(next) == SaveSettings<C>(current)) return;
    std::string error;
    if (!C::IsValid(next, &error) || !component.SetSettings(next)) ImGui::TextWrapped("反映していません: %s", error.c_str());
}
#endif
} // namespace KashipanEngine::GameplayDetail
