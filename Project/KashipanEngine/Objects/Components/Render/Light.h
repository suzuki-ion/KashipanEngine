#pragma once
#include <algorithm>
#include <cstdint>

#include "Debug/Logger.h"
#include "Objects/ObjectComponentHeader.h"
#include "Math/Vector4.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

class Light final : public IObjectComponent {
public:
    /// @brief Rect/Sphere/Disc/Tubeは面光源（形状を持つ光源）。真のLTC等ではなく、
    ///        形状上の代表点（representative point）法で近似する（Objects/Components/Render/AreaLight.hlsli参照）
    /// @details Boxは全方位（体積）発光。ボックス表面上の最近接点を代表点として使い、Tubeと同じ扱いで
    ///          拡散・鏡面・減衰を求める。影もPoint/Sphere/Tubeと同じキューブ6面で近似する
    enum class Type { Directional, Point, Spot, Rect, Sphere, Disc, Tube, Box };
    OBJECT_COMPONENT_CONSTRUCTOR(Light, 0xFF,
        ADD_MEMBER_VARIABLE(color_);
        ADD_MEMBER_VARIABLE(intensity_);
        ADD_MEMBER_VARIABLE(radius_);
        ADD_MEMBER_VARIABLE(distance_);
        ADD_MEMBER_VARIABLE(decay_);
        ADD_MEMBER_VARIABLE(innerAngle_);
        ADD_MEMBER_VARIABLE(outerAngle_);
        ADD_MEMBER_VARIABLE(sourceRadius_);
        ADD_MEMBER_VARIABLE(sourceWidth_);
        ADD_MEMBER_VARIABLE(sourceHeight_);
        ADD_MEMBER_VARIABLE(sourceLength_);
        ADD_MEMBER_VARIABLE(sourceDepth_);
        ADD_MEMBER_VARIABLE(castShadows_);
        ADD_MEMBER_VARIABLE(shadowDistance_);
        ADD_MEMBER_VARIABLE(shadowMapResolution_);
        ADD_MEMBER_VARIABLE(shadowBias_);
        ADD_MEMBER_VARIABLE(shadowSoftness_);
    )
    COMPONENT_CATEGORY("Render")
    ~Light() override = default;
    std::unique_ptr<IObjectComponent> Clone() const override {
        LogScope scope;
        auto ptr = std::make_unique<Light>();
        ptr->type_ = type_;
        ptr->color_ = color_;
        ptr->intensity_ = intensity_;
        ptr->radius_ = radius_;
        ptr->distance_ = distance_;
        ptr->decay_ = decay_;
        ptr->innerAngle_ = innerAngle_;
        ptr->outerAngle_ = outerAngle_;
        ptr->sourceRadius_ = sourceRadius_;
        ptr->sourceWidth_ = sourceWidth_;
        ptr->sourceHeight_ = sourceHeight_;
        ptr->sourceLength_ = sourceLength_;
        ptr->sourceDepth_ = sourceDepth_;
        ptr->castShadows_ = castShadows_;
        ptr->shadowDistance_ = shadowDistance_;
        ptr->shadowMapResolution_ = shadowMapResolution_;
        ptr->shadowBias_ = shadowBias_;
        ptr->shadowSoftness_ = shadowSoftness_;
        return ptr;
    }

    void SetType(Type type) { type_ = type; }
    void SetColor(const Vector4 &color) { color_ = color; }
    void SetIntensity(float intensity) { intensity_ = intensity; }
    /// @brief 減衰距離（届く範囲）を設定する。Point/Sphere/Tubeは球状、Spot/Disc/Rectは片面方向の範囲として使う
    void SetRadius(float radius) { radius_ = radius; }
    /// @brief 減衰距離（届く範囲）を設定する。Spot/Disc/Rect用（Radiusと同じ意味で名前だけ異なる）
    void SetDistance(float distance) { distance_ = distance; }
    void SetDecay(float decay) { decay_ = decay; }
    void SetInnerAngle(float innerAngle) { innerAngle_ = innerAngle; }
    void SetOuterAngle(float outerAngle) { outerAngle_ = outerAngle; }
    /// @brief 面光源の物理的な大きさ（半径）を設定する。Sphere/Discの半径、Tubeの円柱半径として使う
    void SetSourceRadius(float sourceRadius) { sourceRadius_ = sourceRadius; }
    /// @brief Rectの横幅を設定する
    void SetSourceWidth(float sourceWidth) { sourceWidth_ = sourceWidth; }
    /// @brief Rectの縦幅を設定する
    void SetSourceHeight(float sourceHeight) { sourceHeight_ = sourceHeight; }
    /// @brief Tubeの長さ（Transformのローカル+X方向）を設定する
    void SetSourceLength(float sourceLength) { sourceLength_ = sourceLength; }
    /// @brief Boxの奥行き（Transformのローカル+Z方向）を設定する
    void SetSourceDepth(float sourceDepth) { sourceDepth_ = sourceDepth; }
    /// @brief このライトが影を作り出すかを設定
    /// @details Directional=カスケードシャドウ / Spot,Disc,Rect=1面（広角） / Point,Sphere,Tube=キューブ6面
    ///          のシャドウマップが自動生成される（Disc/Rectは片面発光を広角の単一透視投影で近似、
    ///          Tubeは中心点からのキューブマップで近似する）
    void SetCastShadows(bool castShadows) { castShadows_ = castShadows; }
    /// @brief 影を落とす最大距離（Directionalのカスケードシャドウの適用範囲）を設定
    /// @details Spot/Disc/Rectは Distance、Point/Sphere/Tubeは Radius が影の届く範囲になる
    void SetShadowDistance(float shadowDistance) { shadowDistance_ = shadowDistance; }
    /// @brief シャドウマップの解像度（1カスケードあたり）を設定
    void SetShadowMapResolution(std::uint32_t resolution) { shadowMapResolution_ = resolution; }
    /// @brief 影の深度バイアス係数を設定（自動計算されるテクセル基準バイアスへの倍率。1.0が既定）
    /// @details 小さくすると影が実際の位置に近づく代わりにシャドウアクネ（自己遮蔽の縞模様）が出やすくなり、
    ///          大きくすると影がオブジェクトから浮くピーターパン現象が出やすくなる
    void SetShadowBias(float bias) { shadowBias_ = bias; }
    /// @brief Directional/Point/Spotの半影ソフト化に使う光源サイズ（ワールド単位。0で従来通りの硬い影）を設定する
    /// @details Sphere/Disc/Tube/Rectは形状の物理サイズ（GetEffectiveShadowSoftness参照）が自動的に使われるため、
    ///          この値は使われない
    void SetShadowSoftness(float softness) { shadowSoftness_ = softness; }

    Type GetType() const noexcept { return type_; }
    const Vector4 &GetColor() const noexcept { return color_; }
    float GetIntensity() const noexcept { return intensity_; }
    float GetRadius() const noexcept { return radius_; }
    float GetDistance() const noexcept { return distance_; }
    float GetDecay() const noexcept { return decay_; }
    float GetInnerAngle() const noexcept { return innerAngle_; }
    float GetOuterAngle() const noexcept { return outerAngle_; }
    float GetSourceRadius() const noexcept { return sourceRadius_; }
    float GetSourceWidth() const noexcept { return sourceWidth_; }
    float GetSourceHeight() const noexcept { return sourceHeight_; }
    float GetSourceLength() const noexcept { return sourceLength_; }
    float GetSourceDepth() const noexcept { return sourceDepth_; }
    bool IsCastShadows() const noexcept { return castShadows_; }
    float GetShadowDistance() const noexcept { return shadowDistance_; }
    std::uint32_t GetShadowMapResolution() const noexcept { return shadowMapResolution_; }
    float GetShadowBias() const noexcept { return shadowBias_; }
    float GetShadowSoftness() const noexcept { return shadowSoftness_; }

    /// @brief PCSS半影ソフト化に使う実際の光源サイズ（ワールド単位）を取得する
    /// @details Sphere/Disc/Tubeは半径、Rectは対角の半分に相当する値を自動的に返す。
    ///          Directional/Point/Spotはユーザー設定のShadowSoftnessをそのまま返す（既定0=硬い影）
    float GetEffectiveShadowSoftness() const noexcept {
        LogScope scope;
        switch (type_) {
        case Type::Sphere:
        case Type::Tube:
        case Type::Disc:
            return sourceRadius_;
        case Type::Rect:
            return std::max(sourceWidth_, sourceHeight_) * 0.5f;
        case Type::Box:
            return std::max({ sourceWidth_, sourceHeight_, sourceDepth_ }) * 0.5f;
        default:
            return shadowSoftness_;
        }
    }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        LogScope scope;
        int t = static_cast<int>(type_);
        const char *items[] = { "Directional", "Point", "Spot", "Rect", "Sphere", "Disc", "Tube", "Box" };
        if (ImGui::Combo(TranslationLabel("component.light.type"), &t, items, 8)) type_ = static_cast<Type>(t);
        ImGui::ColorEdit4(TranslationLabel("component.light.color"), &color_.x);
        ImGui::DragFloat(TranslationLabel("component.light.intensity"), &intensity_, 0.01f, 0.0f);
        if (type_ == Type::Point) {
            ImGui::DragFloat(TranslationLabel("component.light.radius"), &radius_, 0.1f, 0.0f, 1000.0f);
            ImGui::DragFloat(TranslationLabel("component.light.decay"), &decay_, 0.01f, 0.0f, 10.0f);
        } else if (type_ == Type::Spot) {
            ImGui::DragFloat(TranslationLabel("component.light.distance"), &distance_, 0.1f, 0.0f, 1000.0f);
            ImGui::DragFloat(TranslationLabel("component.light.decay"), &decay_, 0.01f, 0.0f, 10.0f);
            ImGui::SliderAngle(TranslationLabel("component.light.inner_angle"), &innerAngle_, 0.0f, 90.0f);
            ImGui::SliderAngle(TranslationLabel("component.light.outer_angle"), &outerAngle_, 0.0f, 90.0f);
        } else if (type_ == Type::Sphere) {
            ImGui::DragFloat(TranslationLabel("component.light.radius_range"), &radius_, 0.1f, 0.0f, 1000.0f);
            ImGui::DragFloat(TranslationLabel("component.light.source_radius"), &sourceRadius_, 0.01f, 0.001f, 100.0f);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TranslationC("component.light.desc_1"));
            ImGui::DragFloat(TranslationLabel("component.light.decay"), &decay_, 0.01f, 0.0f, 10.0f);
        } else if (type_ == Type::Tube) {
            ImGui::DragFloat(TranslationLabel("component.light.radius_range"), &radius_, 0.1f, 0.0f, 1000.0f);
            ImGui::DragFloat(TranslationLabel("component.light.source_radius"), &sourceRadius_, 0.01f, 0.001f, 100.0f);
            ImGui::DragFloat(TranslationLabel("component.light.source_length"), &sourceLength_, 0.01f, 0.0f, 1000.0f);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TranslationC("component.light.transform_x"));
            ImGui::DragFloat(TranslationLabel("component.light.decay"), &decay_, 0.01f, 0.0f, 10.0f);
        } else if (type_ == Type::Disc) {
            ImGui::DragFloat(TranslationLabel("component.light.distance_range"), &distance_, 0.1f, 0.0f, 1000.0f);
            ImGui::DragFloat(TranslationLabel("component.light.source_radius"), &sourceRadius_, 0.01f, 0.001f, 100.0f);
            ImGui::DragFloat(TranslationLabel("component.light.decay"), &decay_, 0.01f, 0.0f, 10.0f);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TranslationC("component.light.transform_z"));
        } else if (type_ == Type::Rect) {
            ImGui::DragFloat(TranslationLabel("component.light.distance_range"), &distance_, 0.1f, 0.0f, 1000.0f);
            ImGui::DragFloat(TranslationLabel("component.light.source_width"), &sourceWidth_, 0.01f, 0.001f, 100.0f);
            ImGui::DragFloat(TranslationLabel("component.light.source_height"), &sourceHeight_, 0.01f, 0.001f, 100.0f);
            ImGui::DragFloat(TranslationLabel("component.light.decay"), &decay_, 0.01f, 0.0f, 10.0f);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TranslationC("component.light.transform_z_x_y"));
        } else if (type_ == Type::Box) {
            ImGui::DragFloat(TranslationLabel("component.light.radius_range"), &radius_, 0.1f, 0.0f, 1000.0f);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TranslationC("component.light.desc_2"));
            ImGui::DragFloat(TranslationLabel("component.light.source_width"), &sourceWidth_, 0.01f, 0.001f, 100.0f);
            ImGui::DragFloat(TranslationLabel("component.light.source_height"), &sourceHeight_, 0.01f, 0.001f, 100.0f);
            ImGui::DragFloat(TranslationLabel("component.light.source_depth"), &sourceDepth_, 0.01f, 0.001f, 100.0f);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TranslationC("component.light.transform_x_y_z"));
            ImGui::DragFloat(TranslationLabel("component.light.decay"), &decay_, 0.01f, 0.0f, 10.0f);
        }

        ImGui::SeparatorText(TranslationLabel("component.light.shadow"));
        ImGui::Checkbox(TranslationLabel("component.light.cast_shadows"), &castShadows_);
        if (castShadows_) {
            // Shadow DistanceはDirectionalのカスケード適用範囲（Spot/Disc/RectはDistance、Point/Sphere/TubeはRadiusを使用する）
            if (type_ == Type::Directional) {
                ImGui::DragFloat(TranslationLabel("component.light.shadow_distance"), &shadowDistance_, 1.0f, 1.0f, 10000.0f);
            }
            // 解像度は一般的な2のべき乗から選択する
            static const std::uint32_t kResolutions[] = { 512u, 1024u, 2048u, 4096u };
            const char *resolutionItems[] = { "512", "1024", "2048", "4096" };
            int current = 2;
            for (int i = 0; i < 4; ++i) {
                if (kResolutions[i] == shadowMapResolution_) { current = i; break; }
            }
            if (ImGui::Combo(TranslationLabel("component.light.shadow_resolution"), &current, resolutionItems, 4)) {
                shadowMapResolution_ = kResolutions[current];
            }
            ImGui::DragFloat(TranslationLabel("component.light.shadow_bias"), &shadowBias_, 0.01f, 0.0f, 5.0f);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", TranslationC("component.light.desc_3"));
            }
            if (type_ == Type::Directional || type_ == Type::Point || type_ == Type::Spot) {
                ImGui::DragFloat(TranslationLabel("component.light.shadow_softness"), &shadowSoftness_, 0.01f, 0.0f, 100.0f);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", TranslationC("component.light.pcss_n0"));
                }
            } else {
                ImGui::Text(TranslationC("component.light.softness_3f_source_radius"), GetEffectiveShadowSoftness());
            }
        }
    }
#endif
    JSON SaveToJson() const override {
        LogScope scope;
        return JSON{
            {"type", static_cast<int>(type_)}, {"color", ToJSON(color_)}, {"intensity", intensity_},
            {"radius", radius_}, {"distance", distance_}, {"decay", decay_},
            {"innerAngle", innerAngle_}, {"outerAngle", outerAngle_},
            {"sourceRadius", sourceRadius_}, {"sourceWidth", sourceWidth_},
            {"sourceHeight", sourceHeight_}, {"sourceLength", sourceLength_},
            {"sourceDepth", sourceDepth_},
            {"castShadows", castShadows_}, {"shadowDistance", shadowDistance_},
            {"shadowMapResolution", shadowMapResolution_}, {"shadowBias", shadowBias_},
            {"shadowSoftness", shadowSoftness_}
        };
    }
    bool LoadFromJson(const JSON &json) override {
        LogScope scope;
        type_ = static_cast<Type>(json.value("type", 0));
        if (json.contains("color")) color_ = FromJSON<Vector4>(json["color"]);
        intensity_ = json.value("intensity", 1.0f);
        radius_ = json.value("radius", 10.0f);
        distance_ = json.value("distance", 10.0f);
        decay_ = json.value("decay", 2.0f);
        innerAngle_ = json.value("innerAngle", 0.35f);
        outerAngle_ = json.value("outerAngle", 0.6f);
        sourceRadius_ = json.value("sourceRadius", 0.5f);
        sourceWidth_ = json.value("sourceWidth", 1.0f);
        sourceHeight_ = json.value("sourceHeight", 1.0f);
        sourceLength_ = json.value("sourceLength", 1.0f);
        sourceDepth_ = json.value("sourceDepth", 1.0f);
        castShadows_ = json.value("castShadows", false);
        shadowDistance_ = json.value("shadowDistance", 100.0f);
        shadowMapResolution_ = json.value("shadowMapResolution", 2048u);
        shadowBias_ = json.value("shadowBias", 1.0f);
        shadowSoftness_ = json.value("shadowSoftness", 0.0f);
        return true;
    }
private:
    Type type_ = Type::Directional;
    Vector4 color_{ 1.0f, 1.0f, 1.0f, 1.0f };
    float intensity_ = 1.0f;
    // Point/Sphere/Tubeの減衰範囲
    float radius_ = 10.0f;
    // Spot/Disc/Rectの減衰範囲
    float distance_ = 10.0f;
    float innerAngle_ = 0.35f;
    float outerAngle_ = 0.6f;
    // Point/Spot/Rect/Sphere/Disc/Tube共通の減衰指数
    float decay_ = 2.0f;
    // 面光源（Rect/Sphere/Disc/Tube）の物理的な大きさ
    float sourceRadius_ = 0.5f;
    float sourceWidth_ = 1.0f;
    float sourceHeight_ = 1.0f;
    float sourceLength_ = 1.0f;
    // Boxの奥行き（ローカル+Z方向）
    float sourceDepth_ = 1.0f;
    // シャドウ設定
    bool castShadows_ = false;
    float shadowDistance_ = 100.0f;
    std::uint32_t shadowMapResolution_ = 2048;
    /// @brief 自動計算されるテクセル基準の深度バイアスへの倍率（既定1.0）
    float shadowBias_ = 1.0f;
    /// @brief Directional/Point/Spot用の半影ソフト化サイズ（ワールド単位、既定0=硬い影）
    float shadowSoftness_ = 0.0f;
};

REGISTER_COMPONENT_OBJECT(Light)

} // namespace KashipanEngine
