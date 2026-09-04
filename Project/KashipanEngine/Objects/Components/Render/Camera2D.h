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
        ADD_MEMBER_VARIABLE(autoSyncSize_);
        ADD_MEMBER_VARIABLE(pixelSnapping_);
    )
    COMPONENT_CATEGORY("Render")
    ~Camera2D() override = default;
    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Camera2D>();
        ptr->width_ = width_;
        ptr->height_ = height_;
        ptr->nearClip_ = nearClip_;
        ptr->farClip_ = farClip_;
        ptr->autoSyncSize_ = autoSyncSize_;
        ptr->pixelSnapping_ = pixelSnapping_;
        return ptr;
    }

    void SetSize(float width, float height) { width_ = width; height_ = height; }
    void SetNearClip(float nearClip) { nearClip_ = nearClip; }
    void SetFarClip(float farClip) { farClip_ = farClip; }
    /// @brief 同オブジェクトのCameraRendererに描画先が単一指定されている場合、その描画先の実解像度へ
    ///        width/heightを毎フレーム自動追従させるかどうかを設定する
    /// @details 描画先が未指定（全描画先へ適用中）の場合は解像度を一意に決められないため、
    ///          有効にしていても何もしない（手動設定値のまま）
    void SetAutoSyncSize(bool enable) noexcept { autoSyncSize_ = enable; }
    bool GetAutoSyncSize() const noexcept { return autoSyncSize_; }

    /// @brief このカメラを基準に、ピクセルスナップ対象のSpriteRendererを画面ピクセルへ揃えるか設定する
    /// @details カメラ自身のTransformは丸めず、SpriteRendererとカメラの相対座標を描画先解像度の
    ///          ピクセル格子へ丸める。スムーズ追従で両者がサブピクセル移動しても、相対位置が一定なら
    ///          画面上の位置も一定に保たれる。既定は無効
    void SetPixelSnapping(bool enable) noexcept { pixelSnapping_ = enable; }
    bool GetPixelSnapping() const noexcept { return pixelSnapping_; }

    float GetWidth() const noexcept { return width_; }
    float GetHeight() const noexcept { return height_; }
    float GetNearClip() const noexcept { return nearClip_; }
    float GetFarClip() const noexcept { return farClip_; }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::BeginDisabled(autoSyncSize_);
        ImGui::DragFloat(TranslationLabel("component.camera2d.width"), &width_, 1.0f);
        ImGui::DragFloat(TranslationLabel("component.camera2d.height"), &height_, 1.0f);
        ImGui::EndDisabled();
        ImGui::DragFloat(TranslationLabel("component.camera2d.near"), &nearClip_, 0.01f);
        ImGui::DragFloat(TranslationLabel("component.camera2d.far"), &farClip_, 1.0f);
        ImGui::Checkbox(TranslationLabel("component.camera2d.auto_sync_size"), &autoSyncSize_);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TranslationC("component.camera2d.auto_sync_size_desc"));
        }
        ImGui::Checkbox(TranslationLabel("component.camera2d.pixel_snapping"), &pixelSnapping_);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TranslationC("component.camera2d.pixel_snapping_desc"));
        }
    }
#endif
    JSON SaveToJson() const override {
        return JSON{
            {"width", width_}, {"height", height_}, {"nearClip", nearClip_}, {"farClip", farClip_},
            {"autoSyncSize", autoSyncSize_}, {"pixelSnapping", pixelSnapping_}
        };
    }
    bool LoadFromJson(const JSON &json) override {
        width_ = json.value("width", 1280.0f);
        height_ = json.value("height", 720.0f);
        nearClip_ = json.value("nearClip", 0.0f);
        farClip_ = json.value("farClip", 1000.0f);
        autoSyncSize_ = json.value("autoSyncSize", false);
        pixelSnapping_ = json.value("pixelSnapping", false);
        return true;
    }

private:
    float width_ = 1280.0f;
    float height_ = 720.0f;
    float nearClip_ = 0.0f;
    float farClip_ = 1000.0f;
    /// @brief CameraRendererの描画先解像度へwidth/heightを自動追従させるか（既定false）
    bool autoSyncSize_ = false;
    /// @brief このカメラを基準にSpriteRendererを画面ピクセルへスナップするか（既定false）
    bool pixelSnapping_ = false;
};

REGISTER_COMPONENT_OBJECT(Camera2D)

} // namespace KashipanEngine
