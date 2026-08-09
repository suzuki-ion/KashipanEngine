#pragma once
#include "Objects/ObjectComponentHeader.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief 2Dカメラ情報コンポーネント（射影パラメータの保持のみを行う）
class Camera2D final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(Camera2D, 1,
        ADD_MEMBER_VARIABLE(width_);
        ADD_MEMBER_VARIABLE(height_);
        ADD_MEMBER_VARIABLE(nearClip_);
        ADD_MEMBER_VARIABLE(farClip_);
    )
    COMPONENT_CATEGORY("Render")
    ~Camera2D() override = default;
    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Camera2D>();
        ptr->width_ = width_;
        ptr->height_ = height_;
        ptr->nearClip_ = nearClip_;
        ptr->farClip_ = farClip_;
        return ptr;
    }

    void SetSize(float width, float height) { width_ = width; height_ = height; }
    void SetNearClip(float nearClip) { nearClip_ = nearClip; }
    void SetFarClip(float farClip) { farClip_ = farClip; }

    float GetWidth() const noexcept { return width_; }
    float GetHeight() const noexcept { return height_; }
    float GetNearClip() const noexcept { return nearClip_; }
    float GetFarClip() const noexcept { return farClip_; }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::DragFloat(TranslationLabel("component.camera2d.width"), &width_, 1.0f);
        ImGui::DragFloat(TranslationLabel("component.camera2d.height"), &height_, 1.0f);
        ImGui::DragFloat(TranslationLabel("component.camera2d.near"), &nearClip_, 0.01f);
        ImGui::DragFloat(TranslationLabel("component.camera2d.far"), &farClip_, 1.0f);
    }
#endif
    JSON SaveToJson() const override {
        return JSON{ {"width", width_}, {"height", height_}, {"nearClip", nearClip_}, {"farClip", farClip_} };
    }
    bool LoadFromJson(const JSON &json) override {
        width_ = json.value("width", 1280.0f);
        height_ = json.value("height", 720.0f);
        nearClip_ = json.value("nearClip", 0.0f);
        farClip_ = json.value("farClip", 1000.0f);
        return true;
    }

private:
    float width_ = 1280.0f;
    float height_ = 720.0f;
    float nearClip_ = 0.0f;
    float farClip_ = 1000.0f;
};

REGISTER_COMPONENT_OBJECT(Camera2D)

} // namespace KashipanEngine
