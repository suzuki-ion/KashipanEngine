#pragma once
#include <cstdint>

#include "Objects/ObjectComponentHeader.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

class Light final : public IObjectComponent {
public:
    enum class Type { Directional, Point, Spot };
    OBJECT_COMPONENT_CONSTRUCTOR(Light, 0xFF,
        ADD_MEMBER_VARIABLE(color_);
        ADD_MEMBER_VARIABLE(intensity_);
        ADD_MEMBER_VARIABLE(radius_);
        ADD_MEMBER_VARIABLE(distance_);
        ADD_MEMBER_VARIABLE(decay_);
        ADD_MEMBER_VARIABLE(innerAngle_);
        ADD_MEMBER_VARIABLE(outerAngle_);
        ADD_MEMBER_VARIABLE(castShadows_);
        ADD_MEMBER_VARIABLE(shadowDistance_);
        ADD_MEMBER_VARIABLE(shadowMapResolution_);
        ADD_MEMBER_VARIABLE(shadowBias_);
    )
    COMPONENT_CATEGORY("Render")
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
        ptr->castShadows_ = castShadows_;
        ptr->shadowDistance_ = shadowDistance_;
        ptr->shadowMapResolution_ = shadowMapResolution_;
        ptr->shadowBias_ = shadowBias_;
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
    /// @brief このライトが影を作り出すかを設定
    /// @details Directional=カスケードシャドウ / Spot=1面 / Point=キューブ6面 のシャドウマップが自動生成される
    void SetCastShadows(bool castShadows) { castShadows_ = castShadows; }
    /// @brief 影を落とす最大距離（Directionalのカスケードシャドウの適用範囲）を設定
    /// @details Spotは Distance、Pointは Radius が影の届く範囲になる
    void SetShadowDistance(float shadowDistance) { shadowDistance_ = shadowDistance; }
    /// @brief シャドウマップの解像度（1カスケードあたり）を設定
    void SetShadowMapResolution(std::uint32_t resolution) { shadowMapResolution_ = resolution; }
    /// @brief 影の深度バイアス係数を設定（自動計算されるテクセル基準バイアスへの倍率。1.0が既定）
    /// @details 小さくすると影が実際の位置に近づく代わりにシャドウアクネ（自己遮蔽の縞模様）が出やすくなり、
    ///          大きくすると影がオブジェクトから浮くピーターパン現象が出やすくなる
    void SetShadowBias(float bias) { shadowBias_ = bias; }

    Type GetType() const noexcept { return type_; }
    const Vector4 &GetColor() const noexcept { return color_; }
    float GetIntensity() const noexcept { return intensity_; }
    float GetRadius() const noexcept { return radius_; }
    float GetDistance() const noexcept { return distance_; }
    float GetDecay() const noexcept { return decay_; }
    float GetInnerAngle() const noexcept { return innerAngle_; }
    float GetOuterAngle() const noexcept { return outerAngle_; }
    bool IsCastShadows() const noexcept { return castShadows_; }
    float GetShadowDistance() const noexcept { return shadowDistance_; }
    std::uint32_t GetShadowMapResolution() const noexcept { return shadowMapResolution_; }
    float GetShadowBias() const noexcept { return shadowBias_; }

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

        ImGui::SeparatorText("Shadow");
        ImGui::Checkbox("Cast Shadows", &castShadows_);
        if (castShadows_) {
            // Shadow DistanceはDirectionalのカスケード適用範囲（Spot/PointはDistance/Radiusを使用する）
            if (type_ == Type::Directional) {
                ImGui::DragFloat("Shadow Distance", &shadowDistance_, 1.0f, 1.0f, 10000.0f);
            }
            // 解像度は一般的な2のべき乗から選択する
            static const std::uint32_t kResolutions[] = { 512u, 1024u, 2048u, 4096u };
            const char *resolutionItems[] = { "512", "1024", "2048", "4096" };
            int current = 2;
            for (int i = 0; i < 4; ++i) {
                if (kResolutions[i] == shadowMapResolution_) { current = i; break; }
            }
            if (ImGui::Combo("Shadow Resolution", &current, resolutionItems, 4)) {
                shadowMapResolution_ = kResolutions[current];
            }
            ImGui::DragFloat("Shadow Bias", &shadowBias_, 0.01f, 0.0f, 5.0f);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("自動計算されるテクセル基準の深度バイアスへの倍率(既定1.0)。\n下げると影が近づく代わりにシャドウアクネが出やすく、上げると影が浮きやすくなる");
            }
        }
    }
#endif
    JSON SaveToJson() const override {
        return JSON{
            {"type", static_cast<int>(type_)}, {"color", ToJSON(color_)}, {"intensity", intensity_},
            {"radius", radius_}, {"distance", distance_}, {"decay", decay_},
            {"innerAngle", innerAngle_}, {"outerAngle", outerAngle_},
            {"castShadows", castShadows_}, {"shadowDistance", shadowDistance_},
            {"shadowMapResolution", shadowMapResolution_}, {"shadowBias", shadowBias_}
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
        castShadows_ = json.value("castShadows", false);
        shadowDistance_ = json.value("shadowDistance", 100.0f);
        shadowMapResolution_ = json.value("shadowMapResolution", 2048u);
        shadowBias_ = json.value("shadowBias", 1.0f);
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
    // シャドウ設定
    bool castShadows_ = false;
    float shadowDistance_ = 100.0f;
    std::uint32_t shadowMapResolution_ = 2048;
    /// @brief 自動計算されるテクセル基準の深度バイアスへの倍率（既定1.0）
    float shadowBias_ = 1.0f;
};

REGISTER_COMPONENT_OBJECT(Light)

} // namespace KashipanEngine
