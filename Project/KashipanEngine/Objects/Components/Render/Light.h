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
        ptr->radius_ = radius_;
        ptr->distance_ = distance_;
        ptr->decay_ = decay_;
        ptr->innerAngle_ = innerAngle_;
        ptr->outerAngle_ = outerAngle_;
        return ptr;
    }

    void SetType(Type type) { type_ = type; }
    void SetColor(const Vector4 &color) { color_ = color; }
    void SetIntensity(float intensity) { intensity_ = intensity; }
    void SetRadius(float radius) { radius_ = radius; }
    void SetDistance(float distance) { distance_ = distance; }
    void SetDecay(float decay) { decay_ = decay; }
    void SetInnerAngle(float innerAngle) { innerAngle_ = innerAngle; }
    void SetOuterAngle(float outerAngle) { outerAngle_ = outerAngle; }

    Type GetType() const noexcept { return type_; }
    const Vector4 &GetColor() const noexcept { return color_; }
    float GetIntensity() const noexcept { return intensity_; }
    float GetRadius() const noexcept { return radius_; }
    float GetDistance() const noexcept { return distance_; }
    float GetDecay() const noexcept { return decay_; }
    float GetInnerAngle() const noexcept { return innerAngle_; }
    float GetOuterAngle() const noexcept { return outerAngle_; }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        int t = static_cast<int>(type_);
        const char *items[] = { "Directional", "Point", "Spot" };
        if (ImGui::Combo("Type", &t, items, 3)) type_ = static_cast<Type>(t);
        ImGui::ColorEdit4("Color", &color_.x);
        ImGui::DragFloat("Intensity", &intensity_, 0.01f, 0.0f);
        if (type_ == Type::Point) {
            ImGui::DragFloat("Radius", &radius_, 0.1f, 0.0f, 1000.0f);
            ImGui::DragFloat("Decay", &decay_, 0.01f, 0.0f, 10.0f);
        } else if (type_ == Type::Spot) {
            ImGui::DragFloat("Distance", &distance_, 0.1f, 0.0f, 1000.0f);
            ImGui::DragFloat("Decay", &decay_, 0.01f, 0.0f, 10.0f);
            ImGui::SliderAngle("Inner Angle", &innerAngle_, 0.0f, 90.0f);
            ImGui::SliderAngle("Outer Angle", &outerAngle_, 0.0f, 90.0f);
        }
    }
#endif
    JSON SaveToJson() const override {
        return JSON{
            {"type", static_cast<int>(type_)}, {"color", ToJSON(color_)}, {"intensity", intensity_},
            {"radius", radius_}, {"distance", distance_}, {"decay", decay_},
            {"innerAngle", innerAngle_}, {"outerAngle", outerAngle_}
        };
    }
    bool LoadFromJson(const JSON &json) override {
        type_ = static_cast<Type>(json.value("type", 0));
        if (json.contains("color")) color_ = FromJSON<Vector4>(json["color"]);
        intensity_ = json.value("intensity", 1.0f);
        radius_ = json.value("radius", 10.0f);
        distance_ = json.value("distance", 10.0f);
        decay_ = json.value("decay", 2.0f);
        innerAngle_ = json.value("innerAngle", 0.35f);
        outerAngle_ = json.value("outerAngle", 0.6f);
        return true;
    }
private:
    Type type_ = Type::Directional;
    Vector4 color_{ 1.0f, 1.0f, 1.0f, 1.0f };
    float intensity_ = 1.0f;
    // Point用
    float radius_ = 10.0f;
    // Spot用
    float distance_ = 10.0f;
    float innerAngle_ = 0.35f;
    float outerAngle_ = 0.6f;
    // Point/Spot共通
    float decay_ = 2.0f;
};

REGISTER_COMPONENT_OBJECT(Light)

} // namespace KashipanEngine
