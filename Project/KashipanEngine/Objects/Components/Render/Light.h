#pragma once
#include "Objects/ObjectComponentHeader.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

class Light final : public IObjectComponent {
public:
    enum class Type { Directional, Point, Spot };
    OBJECT_COMPONENT_CONSTRUCTOR(Light, 0xFF, )
    ~Light() override = default;
    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Light>();
        ptr->type_ = type_;
        ptr->color_ = color_;
        ptr->intensity_ = intensity_;
        return ptr;
    }

    void SetType(Type type) { type_ = type; }
    void SetColor(const Vector4 &color) { color_ = color; }
    void SetIntensity(float intensity) { intensity_ = intensity; }

    Type GetType() const noexcept { return type_; }
    const Vector4 &GetColor() const noexcept { return color_; }
    float GetIntensity() const noexcept { return intensity_; }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override { int t = static_cast<int>(type_); const char *items[] = { "Directional", "Point", "Spot" }; if (ImGui::Combo("Type", &t, items, 3)) type_ = static_cast<Type>(t); ImGui::ColorEdit4("Color", &color_.x); ImGui::DragFloat("Intensity", &intensity_, 0.01f, 0.0f); }
#endif
    JSON SaveToJson() const override { return JSON{ {"type", static_cast<int>(type_)}, {"color", ToJSON(color_)}, {"intensity", intensity_} }; }
    bool LoadFromJson(const JSON &json) override { type_ = static_cast<Type>(json.value("type", 0)); if (json.contains("color")) color_ = FromJSON<Vector4>(json["color"]); intensity_ = json.value("intensity", 1.0f); return true; }
private:
    Type type_ = Type::Directional;
    Vector4 color_{ 1.0f, 1.0f, 1.0f, 1.0f };
    float intensity_ = 1.0f;
};

REGISTER_COMPONENT_OBJECT(Light)

} // namespace KashipanEngine
