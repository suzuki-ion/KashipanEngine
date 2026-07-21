#include "Scene/Components/Script/ImGuiScriptBindings.h"
#ifdef USE_IMGUI

#include <string>

#include <angelscript.h>
#include <add_on/scriptarray/scriptarray.h>
#include <asbind20/asbind.hpp>

#include <imgui.h>
#include <imgui_stdlib.h>

#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

void RegisterImGuiScriptBindings(asIScriptEngine *engine) {
    if (!engine) return;

    // スクリプト側からは ImGui::Xxx(...) として呼び出す
    asbind20::namespace_ imguiNamespace(engine, "ImGui");
    asbind20::global(engine)
        //--------- テキスト ---------//
        .function("void Text(const string &in)", [](const std::string &text) {
            ImGui::TextUnformatted(text.c_str());
        })
        .function("void TextColored(const Vector4 &in color, const string &in)", [](const Vector4 &color, const std::string &text) {
            ImGui::TextColored(ImVec4(color.x, color.y, color.z, color.w), "%s", text.c_str());
        })
        .function("void TextWrapped(const string &in)", [](const std::string &text) {
            ImGui::TextWrapped("%s", text.c_str());
        })
        .function("void TextDisabled(const string &in)", [](const std::string &text) {
            ImGui::TextDisabled("%s", text.c_str());
        })
        .function("void BulletText(const string &in)", [](const std::string &text) {
            ImGui::BulletText("%s", text.c_str());
        })
        //--------- ボタン・チェックボックス等 ---------//
        .function("bool Button(const string &in, float width = 0, float height = 0)",
            [](const std::string &label, float width, float height) -> bool {
                return ImGui::Button(label.c_str(), ImVec2(width, height));
            })
        .function("bool SmallButton(const string &in)", [](const std::string &label) -> bool {
            return ImGui::SmallButton(label.c_str());
        })
        .function("bool Checkbox(const string &in, bool &inout value)", [](const std::string &label, bool &value) -> bool {
            return ImGui::Checkbox(label.c_str(), &value);
        })
        .function("bool RadioButton(const string &in, bool active)", [](const std::string &label, bool active) -> bool {
            return ImGui::RadioButton(label.c_str(), active);
        })
        .function("bool Selectable(const string &in, bool selected = false)", [](const std::string &label, bool selected) -> bool {
            return ImGui::Selectable(label.c_str(), selected);
        })
        //--------- 入力 ---------//
        .function("bool InputText(const string &in, string &inout text)", [](const std::string &label, std::string &text) -> bool {
            return ImGui::InputText(label.c_str(), &text);
        })
        .function("bool InputTextMultiline(const string &in, string &inout text, float width = 0, float height = 0)",
            [](const std::string &label, std::string &text, float width, float height) -> bool {
                return ImGui::InputTextMultiline(label.c_str(), &text, ImVec2(width, height));
            })
        .function("bool DragInt(const string &in, int &inout value, float speed = 1.0f, int min = 0, int max = 0)",
            [](const std::string &label, int &value, float speed, int min, int max) -> bool {
                return ImGui::DragInt(label.c_str(), &value, speed, min, max);
            })
        .function("bool DragFloat(const string &in, float &inout value, float speed = 1.0f, float min = 0, float max = 0)",
            [](const std::string &label, float &value, float speed, float min, float max) -> bool {
                return ImGui::DragFloat(label.c_str(), &value, speed, min, max);
            })
        .function("bool SliderInt(const string &in, int &inout value, int min, int max)",
            [](const std::string &label, int &value, int min, int max) -> bool {
                return ImGui::SliderInt(label.c_str(), &value, min, max);
            })
        .function("bool SliderFloat(const string &in, float &inout value, float min, float max)",
            [](const std::string &label, float &value, float min, float max) -> bool {
                return ImGui::SliderFloat(label.c_str(), &value, min, max);
            })
        .function("bool DragVector2(const string &in, Vector2 &inout value, float speed = 1.0f)",
            [](const std::string &label, Vector2 &value, float speed) -> bool {
                return ImGui::DragFloat2(label.c_str(), &value.x, speed);
            })
        .function("bool DragVector3(const string &in, Vector3 &inout value, float speed = 1.0f)",
            [](const std::string &label, Vector3 &value, float speed) -> bool {
                return ImGui::DragFloat3(label.c_str(), &value.x, speed);
            })
        .function("bool DragVector4(const string &in, Vector4 &inout value, float speed = 1.0f)",
            [](const std::string &label, Vector4 &value, float speed) -> bool {
                return ImGui::DragFloat4(label.c_str(), &value.x, speed);
            })
        .function("bool ColorEdit3(const string &in, Vector3 &inout color)", [](const std::string &label, Vector3 &color) -> bool {
            return ImGui::ColorEdit3(label.c_str(), &color.x);
        })
        .function("bool ColorEdit4(const string &in, Vector4 &inout color)", [](const std::string &label, Vector4 &color) -> bool {
            return ImGui::ColorEdit4(label.c_str(), &color.x);
        })
        .function("bool Combo(const string &in, int &inout currentIndex, const array<string> &in items)",
            [](const std::string &label, int &currentIndex, const CScriptArray &items) -> bool {
                bool changed = false;
                const int count = static_cast<int>(items.GetSize());
                const auto *preview = (currentIndex >= 0 && currentIndex < count)
                    ? static_cast<const std::string *>(items.At(static_cast<asUINT>(currentIndex))) : nullptr;
                if (ImGui::BeginCombo(label.c_str(), preview ? preview->c_str() : "")) {
                    for (int i = 0; i < count; ++i) {
                        const auto *item = static_cast<const std::string *>(items.At(static_cast<asUINT>(i)));
                        if (!item) continue;
                        const bool selected = (i == currentIndex);
                        if (ImGui::Selectable(item->c_str(), selected)) {
                            currentIndex = i;
                            changed = true;
                        }
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                return changed;
            })
        .function("void ProgressBar(float fraction, const string &in overlay = \"\")",
            [](float fraction, const std::string &overlay) {
                ImGui::ProgressBar(fraction, ImVec2(-1.0f, 0.0f), overlay.empty() ? nullptr : overlay.c_str());
            })
        //--------- レイアウト ---------//
        .function("void Separator()", []() { ImGui::Separator(); })
        .function("void SameLine(float offsetX = 0.0f, float spacing = -1.0f)", [](float offsetX, float spacing) {
            ImGui::SameLine(offsetX, spacing);
        })
        .function("void NewLine()", []() { ImGui::NewLine(); })
        .function("void Spacing()", []() { ImGui::Spacing(); })
        .function("void Indent(float width = 0.0f)", [](float width) { ImGui::Indent(width); })
        .function("void Unindent(float width = 0.0f)", [](float width) { ImGui::Unindent(width); })
        .function("void SetNextItemWidth(float width)", [](float width) { ImGui::SetNextItemWidth(width); })
        .function("void PushID(int id)", [](int id) { ImGui::PushID(id); })
        .function("void PushID(const string &in id)", [](const std::string &id) { ImGui::PushID(id.c_str()); })
        .function("void PopID()", []() { ImGui::PopID(); })
        .function("void BeginDisabled(bool disabled = true)", [](bool disabled) { ImGui::BeginDisabled(disabled); })
        .function("void EndDisabled()", []() { ImGui::EndDisabled(); })
        .function("bool BeginChild(const string &in id, float width = 0, float height = 0, bool border = false)",
            [](const std::string &id, float width, float height, bool border) -> bool {
                return ImGui::BeginChild(id.c_str(), ImVec2(width, height), border);
            })
        .function("void EndChild()", []() { ImGui::EndChild(); })
        //--------- ツリー・折りたたみ ---------//
        .function("bool TreeNode(const string &in)", [](const std::string &label) -> bool {
            return ImGui::TreeNode(label.c_str());
        })
        .function("void TreePop()", []() { ImGui::TreePop(); })
        .function("bool CollapsingHeader(const string &in)", [](const std::string &label) -> bool {
            return ImGui::CollapsingHeader(label.c_str());
        })
        .function("bool InvisibleButton(const string &in id, const Vector2 &in size)",
            [](const std::string &id, const Vector2 &size) -> bool {
                return ImGui::InvisibleButton(id.c_str(), ImVec2(size.x, size.y));
            })
        //--------- 状態取得・その他 ---------//
        .function("bool IsItemHovered()", []() -> bool { return ImGui::IsItemHovered(); })
        .function("bool IsItemClicked(int button = 0)", [](int button) -> bool { return ImGui::IsItemClicked(button); })
        .function("bool IsItemActive()", []() -> bool { return ImGui::IsItemActive(); })
        .function("Vector2 GetMousePos()", []() -> Vector2 {
            const ImVec2 p = ImGui::GetMousePos();
            return Vector2(p.x, p.y);
        })
        .function("void SetTooltip(const string &in)", [](const std::string &text) {
            ImGui::SetTooltip("%s", text.c_str());
        })
        .function("Vector2 GetContentRegionAvail()", []() -> Vector2 {
            const ImVec2 avail = ImGui::GetContentRegionAvail();
            return Vector2(avail.x, avail.y);
        })
        //--------- 図形描画（現在のウィンドウの描画リストへ直接描く。スクリーン座標系） ---------//
        .function("Vector2 GetCursorScreenPos()", []() -> Vector2 {
            const ImVec2 p = ImGui::GetCursorScreenPos();
            return Vector2(p.x, p.y);
        })
        .function("void Dummy(const Vector2 &in size)", [](const Vector2 &size) {
            // 直接描画した領域の分だけ、以降のウィジェットを配置する高さ/幅を確保する
            ImGui::Dummy(ImVec2(size.x, size.y));
        })
        .function("void DrawLine(const Vector2 &in p1, const Vector2 &in p2, const Vector4 &in color, float thickness = 1.0f)",
            [](const Vector2 &p1, const Vector2 &p2, const Vector4 &color, float thickness) {
                ImGui::GetWindowDrawList()->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, color.w)), thickness);
            })
        .function("void DrawRect(const Vector2 &in pMin, const Vector2 &in pMax, const Vector4 &in color, float thickness = 1.0f, float rounding = 0.0f)",
            [](const Vector2 &pMin, const Vector2 &pMax, const Vector4 &color, float thickness, float rounding) {
                ImGui::GetWindowDrawList()->AddRect(ImVec2(pMin.x, pMin.y), ImVec2(pMax.x, pMax.y),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, color.w)), rounding, 0, thickness);
            })
        .function("void DrawRectFilled(const Vector2 &in pMin, const Vector2 &in pMax, const Vector4 &in color, float rounding = 0.0f)",
            [](const Vector2 &pMin, const Vector2 &pMax, const Vector4 &color, float rounding) {
                ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(pMin.x, pMin.y), ImVec2(pMax.x, pMax.y),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, color.w)), rounding);
            })
        .function("void DrawCircleFilled(const Vector2 &in center, float radius, const Vector4 &in color, int segments = 0)",
            [](const Vector2 &center, float radius, const Vector4 &color, int segments) {
                ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(center.x, center.y), radius,
                    ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, color.w)), segments);
            })
        .function("void DrawText(const Vector2 &in pos, const Vector4 &in color, const string &in text)",
            [](const Vector2 &pos, const Vector4 &color, const std::string &text) {
                ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x, pos.y),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, color.w)), text.c_str());
            });
}

} // namespace KashipanEngine
#endif // USE_IMGUI
