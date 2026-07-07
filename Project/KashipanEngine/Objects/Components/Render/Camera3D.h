#pragma once
#include "Objects/ObjectComponentHeader.h"

namespace KashipanEngine {

/// @brief 3Dカメラ情報コンポーネント（射影パラメータの保持のみを行う）
class Camera3D final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(Camera3D, 1, )
    COMPONENT_CATEGORY("Render")
    ~Camera3D() override = default;
    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Camera3D>();
        ptr->fovY_ = fovY_;
        ptr->nearClip_ = nearClip_;
        ptr->farClip_ = farClip_;
        ptr->aspectRatio_ = aspectRatio_;
        ptr->orthographic_ = orthographic_;
        ptr->orthoSize_ = orthoSize_;
        return ptr;
    }

    void SetFovY(float fovY) { fovY_ = fovY; }
    void SetNearClip(float nearClip) { nearClip_ = nearClip; }
    void SetFarClip(float farClip) { farClip_ = farClip; }
    void SetAspectRatio(float aspectRatio) { aspectRatio_ = aspectRatio; }
    void SetOrthographic(bool orthographic) { orthographic_ = orthographic; }
    void SetOrthoSize(float orthoSize) { orthoSize_ = orthoSize; }

    float GetFovY() const noexcept { return fovY_; }
    float GetNearClip() const noexcept { return nearClip_; }
    float GetFarClip() const noexcept { return farClip_; }
    float GetAspectRatio() const noexcept { return aspectRatio_; }
    bool IsOrthographic() const noexcept { return orthographic_; }
    float GetOrthoSize() const noexcept { return orthoSize_; }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::Checkbox("Orthographic", &orthographic_);
        ImGui::DragFloat("FovY", &fovY_, 0.01f);
        ImGui::DragFloat("Near", &nearClip_, 0.01f);
        ImGui::DragFloat("Far", &farClip_, 1.0f);
        ImGui::DragFloat("Aspect", &aspectRatio_, 0.01f);
        ImGui::DragFloat("OrthoSize", &orthoSize_, 0.1f);
    }
#endif
    JSON SaveToJson() const override {
        return JSON{
            {"fovY", fovY_}, {"nearClip", nearClip_}, {"farClip", farClip_},
            {"aspectRatio", aspectRatio_}, {"orthographic", orthographic_}, {"orthoSize", orthoSize_}
        };
    }
    bool LoadFromJson(const JSON &json) override {
        fovY_ = json.value("fovY", 0.45f);
        nearClip_ = json.value("nearClip", 0.1f);
        farClip_ = json.value("farClip", 1000.0f);
        aspectRatio_ = json.value("aspectRatio", 16.0f / 9.0f);
        orthographic_ = json.value("orthographic", false);
        orthoSize_ = json.value("orthoSize", 10.0f);
        return true;
    }

private:
    float fovY_ = 0.45f;
    float nearClip_ = 0.1f;
    float farClip_ = 1000.0f;
    float aspectRatio_ = 16.0f / 9.0f;
    bool orthographic_ = false;
    /// @brief 平行投影時の縦半径（ワールド単位）
    float orthoSize_ = 10.0f;
};

REGISTER_COMPONENT_OBJECT(Camera3D)

} // namespace KashipanEngine
