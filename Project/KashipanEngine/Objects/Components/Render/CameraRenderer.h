#pragma once
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/Render/Camera2D.h"
#include "Objects/Components/Render/Camera3D.h"
#include "Objects/Components/Render/IWindowObjectComponent.h"
#include "Objects/Components/Render/NormalWindowObject.h"
#include "Objects/Components/Render/OverlayWindowObject.h"
#include "Objects/Components/Render/ScreenBufferObject.h"
#include "Objects/Components/Shake.h"
#include "Objects/Components/Transform.h"
#include "Graphics/IRenderTarget.h"
#include "Graphics/Resources/ConstantBufferResource.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Graphics/PipelineManager.h"
#include "Scene/Components/Render/SceneRenderer.h"
#include "Utilities/UUID128.h"
#if defined(USE_IMGUI)
#include "Objects/Components/Render/TargetObjectSelector.h"
#include "Utilities/Translation.h"
#endif

namespace KashipanEngine {

/// @brief 描画時に使用するカメラへ付与するコンポーネント
/// @details 同一オブジェクトの Transform と Camera3D（または Camera2D）から
///          ビュー射影行列を計算して定数バッファへアップロードする。
///          使用するパイプラインと、そのパイプラインに含まれるシェーダーの
///          どの定数バッファへバインドするかを指定できる。
class CameraRenderer final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(CameraRenderer, 0xFF, SetUpdatePriority(950);)
    COMPONENT_CATEGORY("Render")
    ~CameraRenderer() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<CameraRenderer>();
        ptr->targetObjectID_ = targetObjectID_;
        ptr->pipelineName_ = pipelineName_;
        ptr->bindVariableNames_ = bindVariableNames_;
        ptr->excludedRenderTargetNames_ = excludedRenderTargetNames_;
        return ptr;
    }

    /// @brief 使用するパイプラインを設定（空文字の場合は全パイプラインに適用）
    void SetPipelineName(const std::string &pipelineName) { pipelineName_ = pipelineName; }
    const std::string &GetPipelineName() const noexcept { return pipelineName_; }

    //==================================================
    // 描画先指定
    //==================================================

    /// @brief 適用先の描画先オブジェクトを設定（未指定の場合は全描画先に適用）
    void SetTargetObject(const EmptyObject *targetObject) {
        targetObjectID_ = targetObject ? targetObject->GetObjectID() : UUID128();
    }
    /// @brief 適用先の描画先オブジェクトをUUIDから設定
    void SetTargetObject(const UUID128 &targetObjectID) { targetObjectID_ = targetObjectID; }
    /// @brief 適用先の描画先オブジェクトのUUIDを取得
    const UUID128 &GetTargetObjectID() const noexcept { return targetObjectID_; }
    /// @brief 適用先の描画先オブジェクトを取得（未指定・存在しない場合は nullptr）
    EmptyObject *GetTargetObject() const {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext || !targetObjectID_.IsValid()) return nullptr;
        return sceneContext->GetSceneObject(targetObjectID_);
    }

    /// @brief 指定の描画先がこのコンポーネントの適用対象に含まれるか（除外設定されていないか）
    bool IsRenderTargetIncluded(const IRenderTarget *target) const {
        if (!target) return false;
        return !excludedRenderTargetNames_.contains(target->GetRenderTargetName());
    }

    /// @brief バインド先の定数バッファ変数名を設定（例: "Vertex:gCamera3D"）
    void SetBindVariableNames(const std::vector<std::string> &names) { bindVariableNames_ = names; }
    const std::vector<std::string> &GetBindVariableNames() const noexcept { return bindVariableNames_; }

    /// @brief カメラ情報の定数バッファを取得
    ConstantBufferResource *GetConstantBuffer() const noexcept { return constantBuffer_.get(); }

    /// @brief カメラのワールド座標を取得（Transform が無い場合は原点）
    Vector3 GetWorldPosition() const {
        const Matrix4x4 world = GetRenderWorldMatrix();
        return Vector3(world.m[3][0], world.m[3][1], world.m[3][2]);
    }

    /// @brief 最後にアップロードしたビュー射影行列を取得（シャドウマップ定数等に使用）
    const Matrix4x4 &GetViewProjectionMatrix() const noexcept { return lastViewProjection_; }
    /// @brief ニアクリップ距離を取得
    float GetNearClip() const noexcept { return lastNearClip_; }
    /// @brief ファークリップ距離を取得
    float GetFarClip() const noexcept { return lastFarClip_; }

    /// @brief カメラの定数バッファを現在のTransformの状態で更新する
    /// @details ゲームループが停止/一時停止中でも描画自体は継続されるため、
    ///          Update() ではなく Renderer から毎フレーム直接呼ばれる（プル型の更新）
    void RefreshConstantBuffer() { UploadCameraConstant(); }

protected:
    void Initialize() override {
        if (!constantBuffer_) {
            constantBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(Camera3DConstant));
        }
        // バインド先が未指定の場合はカメラの種類から既定値を設定
        if (bindVariableNames_.empty()) {
            auto *objectContext = GetOwnerObjectContext();
            if (objectContext && objectContext->GetComponent<Camera2D>()) {
                bindVariableNames_ = { "Vertex:gCamera2D", "Pixel:gCamera2D" };
            } else {
                bindVariableNames_ = { "Vertex:gCamera3D", "Pixel:gCamera3D" };
            }
        }
        auto *sceneRenderer = GetOrAddSceneRenderer();
        if (sceneRenderer) {
            sceneRenderer->RegisterCameraRenderer(this);
        }
        UploadCameraConstant();
    }

    void Finalize() override {
        auto *sceneContext = GetOwnerSceneContext();
        auto *sceneRenderer = sceneContext ? sceneContext->GetComponent<SceneRenderer>() : nullptr;
        if (sceneRenderer) {
            sceneRenderer->UnregisterCameraRenderer(this);
        }
        constantBuffer_.reset();
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        // 適用先の描画先オブジェクトをシーン上から選択（D&D対応、未指定は全描画先）
        TargetObjectSelector::ShowSelector(TranslationLabel("component.common.target"), GetOwnerSceneContext(), targetObjectID_);
        // 対象オブジェクトが持つ描画先ごとに適用する/しないを選択する
        TargetObjectSelector::ShowRenderTargetFilters(GetOwnerSceneContext(), targetObjectID_, excludedRenderTargetNames_);
        // パイプラインは読み込み済みのものから選択（未指定は全パイプライン）
        ImGuiCustom::SelectString(TranslationLabel("component.camerarenderer.pipeline"), pipelineName_, PipelineManager::GetLoadedRenderPipelineNames(), true);
        for (const auto &name : bindVariableNames_) {
            ImGui::BulletText("%s", name.c_str());
        }
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["targetObjectID"] = ToJSON(targetObjectID_);
        json["pipelineName"] = pipelineName_;
        json["bindVariableNames"] = bindVariableNames_;
        for (const auto &name : excludedRenderTargetNames_) {
            json["excludedRenderTargetNames"].push_back(name);
        }
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        if (json.contains("targetObjectID")) {
            targetObjectID_ = FromJSON<UUID128>(json["targetObjectID"]);
        } else {
            targetObjectID_ = UUID128();
        }
        pipelineName_ = json.value("pipelineName", std::string{});
        bindVariableNames_.clear();
        if (json.contains("bindVariableNames")) {
            for (const auto &name : json["bindVariableNames"]) {
                bindVariableNames_.push_back(name.get<std::string>());
            }
        }
        excludedRenderTargetNames_.clear();
        for (const auto &name : json.value("excludedRenderTargetNames", std::vector<std::string>())) {
            excludedRenderTargetNames_.insert(name);
        }
        return true;
    }

private:
    /// @brief gCamera3D 定数バッファと同レイアウトの構造体
    struct Camera3DConstant {
        Matrix4x4 view;
        Matrix4x4 projection;
        Matrix4x4 viewProjection;
        Vector4 eyePosition;
        float fov = 0.0f;
        float padding[3]{};
    };
    /// @brief gCamera2D 定数バッファと同レイアウトの構造体
    struct Camera2DConstant {
        Matrix4x4 view;
        Matrix4x4 projection;
        Matrix4x4 viewProjection;
    };

    SceneRenderer *GetOrAddSceneRenderer() const {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext) return nullptr;
        auto *sceneRenderer = sceneContext->GetComponent<SceneRenderer>();
        if (!sceneRenderer) {
            sceneRenderer = sceneContext->AddComponent<SceneRenderer>();
        }
        return sceneRenderer;
    }

    /// @brief Transformを変更せず、RenderOnlyシェイクを反映した描画用ワールド行列を取得する
    Matrix4x4 GetRenderWorldMatrix() const {
        auto *objectContext = GetOwnerObjectContext();
        auto *transform = objectContext ? objectContext->GetComponent<Transform>() : nullptr;
        const Matrix4x4 world = transform ? transform->GetWorldMatrix() : Matrix4x4::Identity();
        return Shake::ApplyRenderOnlyOffsets(GetOwnerObject(), world);
    }

    /// @brief 適用先の描画先オブジェクトから実解像度(px)を取得する（Camera2D/Camera3Dの自動追従に使う）
    /// @details 描画先が単一で明示指定されている場合のみ解決する。未指定（全描画先へ適用中）の場合は
    ///          描画先ごとに解像度が異なりうるため一意に決められず false を返す
    bool ResolveTargetRenderTargetSize(std::uint32_t &outWidth, std::uint32_t &outHeight) const {
        EmptyObject *targetObj = GetTargetObject();
        if (!targetObj) return false;

        IRenderTarget *renderTarget = nullptr;
        if (auto *normalWindow = targetObj->GetComponent<NormalWindowObject>()) {
            // window_はウィンドウがユーザー操作等で閉じられた後もコンポーネント側に残り得る
            // （Finalize経由で破棄された場合のみクリアされる）生ポインタのため、
            // 破棄済みの場合はWindow::IsExist()で弾いてからでないとダングリングポインタを
            // 経由した仮想関数呼び出し（IsRenderTargetAvailable）でクラッシュする
            Window *window = normalWindow->GetWindow();
            if (!window || !Window::IsExist(window)) return false;
            renderTarget = window;
        } else if (auto *overlayWindow = targetObj->GetComponent<OverlayWindowObject>()) {
            Window *window = overlayWindow->GetWindow();
            if (!window || !Window::IsExist(window)) return false;
            renderTarget = window;
        } else if (auto *screenBuffer = targetObj->GetComponent<ScreenBufferObject>()) {
            ScreenBuffer *buffer = screenBuffer->GetScreenBuffer();
            if (!buffer || !ScreenBuffer::IsExist(buffer)) return false;
            renderTarget = buffer;
        }
        if (!renderTarget || !renderTarget->IsRenderTargetAvailable()) return false;

        outWidth = renderTarget->GetRenderTargetWidth();
        outHeight = renderTarget->GetRenderTargetHeight();
        return outWidth > 0 && outHeight > 0;
    }

    void UploadCameraConstant() {
        if (!constantBuffer_) return;
        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return;

        const Matrix4x4 world = GetRenderWorldMatrix();
        const Matrix4x4 view = world.Inverse();

        void *mapped = constantBuffer_->Map();
        if (!mapped) return;

        if (auto *camera2d = objectContext->GetComponent<Camera2D>()) {
            if (camera2d->GetAutoSyncSize()) {
                std::uint32_t targetWidth = 0, targetHeight = 0;
                if (ResolveTargetRenderTargetSize(targetWidth, targetHeight)) {
                    camera2d->SetSize(static_cast<float>(targetWidth), static_cast<float>(targetHeight));
                }
            }
            Camera2DConstant constant{};
            constant.view = view;
            constant.projection.MakeOrthographicMatrix(
                0.0f, 0.0f, camera2d->GetWidth(), camera2d->GetHeight(),
                camera2d->GetNearClip(), camera2d->GetFarClip());
            constant.viewProjection = constant.view * constant.projection;
            lastViewProjection_ = constant.viewProjection;
            lastNearClip_ = camera2d->GetNearClip();
            lastFarClip_ = camera2d->GetFarClip();
            std::memcpy(mapped, &constant, sizeof(constant));
            return;
        }

        Camera3DConstant constant{};
        constant.view = view;
        float fovY = 0.45f;
        float nearClip = 0.1f;
        float farClip = 1000.0f;
        float aspect = 16.0f / 9.0f;
        bool orthographic = false;
        float orthoSize = 10.0f;
        auto *camera3d = objectContext->GetComponent<Camera3D>();
        if (camera3d) {
            if (camera3d->GetAutoSyncAspectRatio()) {
                std::uint32_t targetWidth = 0, targetHeight = 0;
                if (ResolveTargetRenderTargetSize(targetWidth, targetHeight)) {
                    camera3d->SetAspectRatio(static_cast<float>(targetWidth) / static_cast<float>(targetHeight));
                }
            }
            fovY = camera3d->GetFovY();
            nearClip = camera3d->GetNearClip();
            farClip = camera3d->GetFarClip();
            aspect = camera3d->GetAspectRatio();
            orthographic = camera3d->IsOrthographic();
            orthoSize = camera3d->GetOrthoSize();
        }
        if (orthographic) {
            constant.projection.MakeOrthographicMatrix(
                -orthoSize * aspect, orthoSize, orthoSize * aspect, -orthoSize, nearClip, farClip);
        } else {
            constant.projection.MakePerspectiveFovMatrix(fovY, aspect, nearClip, farClip);
        }
        // シャドウのカスケードフィッティングやエディタのピッキング、スクリプトへ公開している
        // GetViewProjectionMatrix()は、ジッターの影響を受けないこちらの値を参照する
        lastViewProjection_ = constant.view * constant.projection;
        lastNearClip_ = nearClip;
        lastFarClip_ = farClip;

        if (camera3d && camera3d->IsJitterEnabled()) {
            camera3d->ApplyProjectionJitter(constant.projection);
        }

        constant.viewProjection = constant.view * constant.projection;
        constant.eyePosition = Vector4(world.m[3][0], world.m[3][1], world.m[3][2], 1.0f);
        constant.fov = fovY;
        std::memcpy(mapped, &constant, sizeof(constant));
    }

    std::unique_ptr<ConstantBufferResource> constantBuffer_;
    UUID128 targetObjectID_{};
    std::string pipelineName_;
    std::vector<std::string> bindVariableNames_;
    /// @brief 除外する描画先の名前（GetRenderTargetName()）の集合
    std::unordered_set<std::string> excludedRenderTargetNames_;

    // シャドウマップ定数等で参照するためのキャッシュ
    Matrix4x4 lastViewProjection_ = Matrix4x4::Identity();
    float lastNearClip_ = 0.1f;
    float lastFarClip_ = 1000.0f;
};

REGISTER_COMPONENT_OBJECT(CameraRenderer)

} // namespace KashipanEngine
