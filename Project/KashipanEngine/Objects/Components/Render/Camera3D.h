#pragma once
#include <cstdint>

#include "Math/Matrix4x4.h"
#include "Objects/ObjectComponentHeader.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief 3Dカメラ情報コンポーネント（射影パラメータの保持のみを行う）
/// @details CameraRenderer（通常のシーンカメラ）とSceneEditorView（シーンビュー専用の
///          エディタカメラ）の両方から参照される共通データのため、カメラジッターの
///          オン/オフや位相もこのコンポーネント側に持たせている
class Camera3D final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(Camera3D, 1,
        ADD_MEMBER_VARIABLE(fovY_);
        ADD_MEMBER_VARIABLE(nearClip_);
        ADD_MEMBER_VARIABLE(farClip_);
        ADD_MEMBER_VARIABLE(aspectRatio_);
        ADD_MEMBER_VARIABLE(orthographic_);
        ADD_MEMBER_VARIABLE(orthoSize_);
        ADD_MEMBER_VARIABLE(enableJitter_);
        ADD_MEMBER_VARIABLE(autoSyncAspectRatio_);
    )
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
        ptr->enableJitter_ = enableJitter_;
        ptr->autoSyncAspectRatio_ = autoSyncAspectRatio_;
        return ptr;
    }

    void SetFovY(float fovY) { fovY_ = fovY; }
    void SetNearClip(float nearClip) { nearClip_ = nearClip; }
    void SetFarClip(float farClip) { farClip_ = farClip; }
    void SetAspectRatio(float aspectRatio) { aspectRatio_ = aspectRatio; }
    void SetOrthographic(bool orthographic) { orthographic_ = orthographic; }
    void SetOrthoSize(float orthoSize) { orthoSize_ = orthoSize; }
    /// @brief 同オブジェクトのCameraRendererに描画先が単一指定されている場合、その描画先の実解像度から
    ///        求めたアスペクト比へ毎フレーム自動追従させるかどうかを設定する
    /// @details 描画先が未指定（全描画先へ適用中）の場合は解像度を一意に決められないため、
    ///          有効にしていても何もしない（手動設定値のまま）
    void SetAutoSyncAspectRatio(bool enable) noexcept { autoSyncAspectRatio_ = enable; }
    bool GetAutoSyncAspectRatio() const noexcept { return autoSyncAspectRatio_; }

    float GetFovY() const noexcept { return fovY_; }
    float GetNearClip() const noexcept { return nearClip_; }
    float GetFarClip() const noexcept { return farClip_; }
    float GetAspectRatio() const noexcept { return aspectRatio_; }
    bool IsOrthographic() const noexcept { return orthographic_; }
    float GetOrthoSize() const noexcept { return orthoSize_; }

    //==================================================
    // カメラジッター（TemporalBlendEffectと組み合わせて使うサブピクセル揺らし）
    //==================================================

    /// @brief 投影行列にサブピクセル単位のジッターを乗せるかどうかを設定する
    /// @details TemporalBlendEffect等のフレーム蓄積と組み合わせない場合、単にジッターの分だけ
    ///          画面が揺れて見える（蓄積側で打ち消されない）ため、単独では有効化しないこと
    void SetEnableJitter(bool enable) { enableJitter_ = enable; }
    bool IsJitterEnabled() const noexcept { return enableJitter_; }

    /// @brief 投影行列にサブピクセル単位のNDCオフセットを加算する（TAA的なカメラジッター）
    /// @details 呼び出し側（CameraRenderer/SceneEditorView）は、GPUへアップロードする投影行列
    ///          にのみこれを適用し、シャドウのカスケードフィッティングやピッキングに使う
    ///          ビュー射影行列は非ジッターのまま別に保持すること。
    ///          このカメラが複数の描画先（解像度が異なりうる）で共有されうるため、ここでは
    ///          実際の描画先解像度を問い合わせず、アスペクト比から仮定した代表解像度で
    ///          サブピクセル幅を近似する。実解像度と多少ずれてもジッター量が意図よりわずかに
    ///          大小するだけで実害は無い
    void ApplyProjectionJitter(Matrix4x4 &projection) {
        // Halton(2,3)列を8点周期で回す。0番目は(0,0)で偏るため1から使う
        constexpr std::uint32_t kJitterPeriod = 8;
        const std::uint32_t index = (jitterIndex_ % kJitterPeriod) + 1;
        ++jitterIndex_;
        const float hx = HaltonSequence(index, 2) - 0.5f;
        const float hy = HaltonSequence(index, 3) - 0.5f;

        constexpr float kJitterReferenceHeight = 1080.0f;
        const float referenceWidth = kJitterReferenceHeight * aspectRatio_;
        const float jitterX = hx * (2.0f / referenceWidth);
        const float jitterY = hy * (2.0f / kJitterReferenceHeight);

        if (orthographic_) {
            // 平行投影ではNDCオフセットは平行移動行（4行目）に直接乗っている
            projection.m[3][0] += jitterX;
            projection.m[3][1] += jitterY;
        } else {
            // 透視投影ではview空間zがそのままwになる行（3行目）へ加算することで、
            // 透視除算後に深度へ依らず一定のNDCオフセットになる
            projection.m[2][0] += jitterX;
            projection.m[2][1] += jitterY;
        }
    }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::Checkbox(TranslationLabel("component.camera3d.orthographic"), &orthographic_);
        ImGui::DragFloat(TranslationLabel("component.camera3d.fovy"), &fovY_, 0.01f);
        ImGui::DragFloat(TranslationLabel("component.camera3d.near"), &nearClip_, 0.01f);
        ImGui::DragFloat(TranslationLabel("component.camera3d.far"), &farClip_, 1.0f);
        ImGui::DragFloat(TranslationLabel("component.camera3d.aspect"), &aspectRatio_, 0.01f);
        ImGui::DragFloat(TranslationLabel("component.camera3d.orthosize"), &orthoSize_, 0.1f);
        ImGui::Checkbox(TranslationLabel("component.camera3d.enable_jitter"), &enableJitter_);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TranslationC("component.camera3d.enable_jitter_desc"));
        }
        ImGui::Checkbox(TranslationLabel("component.camera3d.auto_sync_aspect"), &autoSyncAspectRatio_);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TranslationC("component.camera3d.auto_sync_aspect_desc"));
        }
    }
#endif
    JSON SaveToJson() const override {
        return JSON{
            {"fovY", fovY_}, {"nearClip", nearClip_}, {"farClip", farClip_},
            {"aspectRatio", aspectRatio_}, {"orthographic", orthographic_}, {"orthoSize", orthoSize_},
            {"enableJitter", enableJitter_}, {"autoSyncAspectRatio", autoSyncAspectRatio_}
        };
    }
    bool LoadFromJson(const JSON &json) override {
        fovY_ = json.value("fovY", 0.45f);
        nearClip_ = json.value("nearClip", 0.1f);
        farClip_ = json.value("farClip", 1000.0f);
        aspectRatio_ = json.value("aspectRatio", 16.0f / 9.0f);
        orthographic_ = json.value("orthographic", false);
        orthoSize_ = json.value("orthoSize", 10.0f);
        enableJitter_ = json.value("enableJitter", false);
        autoSyncAspectRatio_ = json.value("autoSyncAspectRatio", false);
        return true;
    }

private:
    /// @brief Halton(base)列のindex番目の値を[0,1)で返す（カメラジッター用の低差異乱数）
    static float HaltonSequence(std::uint32_t index, std::uint32_t base) noexcept {
        float result = 0.0f;
        float f = 1.0f / static_cast<float>(base);
        std::uint32_t i = index;
        while (i > 0) {
            result += f * static_cast<float>(i % base);
            i /= base;
            f /= static_cast<float>(base);
        }
        return result;
    }

    float fovY_ = 0.45f;
    float nearClip_ = 0.1f;
    float farClip_ = 1000.0f;
    float aspectRatio_ = 16.0f / 9.0f;
    bool orthographic_ = false;
    /// @brief 平行投影時の縦半径（ワールド単位）
    float orthoSize_ = 10.0f;

    /// @brief 投影行列にサブピクセルジッターを乗せるか（TemporalBlendEffectとの併用を想定）
    bool enableJitter_ = false;
    /// @brief ジッター列（Halton(2,3)）の現在位置。Clone/Save/Loadの対象外（実行時状態のみ）
    std::uint32_t jitterIndex_ = 0;
    /// @brief CameraRendererの描画先解像度からアスペクト比を自動追従させるか（既定false）
    bool autoSyncAspectRatio_ = false;
};

REGISTER_COMPONENT_OBJECT(Camera3D)

} // namespace KashipanEngine
