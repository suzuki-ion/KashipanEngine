#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Assets/MaterialManager.h"
#include "Assets/ModelManager.h"
#include "Assets/TextureManager.h"
#include "Core/Window.h"
#include "Graphics/IRenderTarget.h"
#include "Graphics/ScreenBuffer.h"
#include "Input/Input.h"
#include "Input/Mouse.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Objects/Components/MeshFilter.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/Render/Camera2D.h"
#include "Objects/Components/Render/IWindowObjectComponent.h"
#include "Objects/Components/Render/NormalWindowObject.h"
#include "Objects/Components/Render/OverlayWindowObject.h"
#include "Objects/Components/Render/ScreenBufferObject.h"
#include "Objects/Components/Render/SpriteRenderer.h"
#include "Scene/Components/Render/SceneRenderer.h"
#include "Utilities/UUID128.h"
#if defined(USE_IMGUI)
#include "Objects/Components/Render/TargetObjectSelector.h"
#include "Utilities/Translation.h"
#endif

namespace KashipanEngine {

/// @brief ScreenBufferObjectの内容を、指定した描画先（Windowなど）へ2Dスプライトとして表示するコンポーネント
/// @details 内部でMeshFilter（"Rect2D"固定）とSpriteRendererを自動生成・管理し、
///          ScreenBufferの内容をテクスチャとして同期し続ける。新規の描画コードは持たず、
///          既存のSpriteRenderer描画パイプラインにそのまま乗せる薄いラッパー。
///          MeshFilter/SpriteRendererはこのコンポーネントを削除しても自動では削除されない
///          （他コンポーネントと同様、ユーザーが明示的に削除可能な通常コンポーネントとして扱う）。
///          MeshFilterは1オブジェクトにつき1つまでのため、専用オブジェクトへの付与を推奨する。
///          表示先の指定はSpriteRenderer側のSetTargetObject（GetSpriteRenderer()経由）に委譲する。
class ScreenBufferViewport final : public IObjectComponent {
public:
    /// @brief 表示先（描画先）の解像度に対する自動フィット方法
    enum class FitMode : int {
        None = 0,   // 何もしない（Transform.Scaleを手動設定した値のまま使う）
        Stretch,    // アスペクト比を無視して描画先いっぱいに引き伸ばす
        Letterbox,  // アスペクト比を保ったまま描画先に収まる最大サイズにする（余白ができる）
    };

    OBJECT_COMPONENT_CONSTRUCTOR(ScreenBufferViewport, 1, )
    COMPONENT_CATEGORY("Render")
    ~ScreenBufferViewport() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<ScreenBufferViewport>();
        ptr->sourceObjectID_ = sourceObjectID_;
        ptr->displayCameraObjectID_ = displayCameraObjectID_;
        ptr->fitMode_ = fitMode_;
        return ptr;
    }

    //==================================================
    // 参照設定（ソース＝映す内容、表示カメラ＝マウス座標変換の基準）
    //==================================================

    /// @brief 表示するScreenBufferObjectを持つオブジェクトを設定
    void SetSourceObject(const EmptyObject *sourceObject) {
        sourceObjectID_ = sourceObject ? sourceObject->GetObjectID() : UUID128();
    }
    void SetSourceObject(const UUID128 &sourceObjectID) { sourceObjectID_ = sourceObjectID; }
    const UUID128 &GetSourceObjectID() const noexcept { return sourceObjectID_; }
    EmptyObject *GetSourceObject() const {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext || !sourceObjectID_.IsValid()) return nullptr;
        return sceneContext->GetSceneObject(sourceObjectID_);
    }

    /// @brief マウス座標変換の基準にするCamera2Dを持つオブジェクトを設定
    /// @details 描画先（どのWindowに映すか）はSpriteRenderer側（GetSpriteRenderer()->SetTargetObject）
    ///          で指定する。こちらは「その描画結果を、どのカメラ視点で見た時のマウス座標に変換するか」の指定
    void SetDisplayCameraObject(const EmptyObject *cameraObject) {
        displayCameraObjectID_ = cameraObject ? cameraObject->GetObjectID() : UUID128();
    }
    void SetDisplayCameraObject(const UUID128 &cameraObjectID) { displayCameraObjectID_ = cameraObjectID; }
    const UUID128 &GetDisplayCameraObjectID() const noexcept { return displayCameraObjectID_; }
    EmptyObject *GetDisplayCameraObject() const {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext || !displayCameraObjectID_.IsValid()) return nullptr;
        return sceneContext->GetSceneObject(displayCameraObjectID_);
    }

    /// @brief 内部で自動管理しているSpriteRendererを取得する
    /// @details 描画先（SetTargetObject）・アンカー/ピボット・レンダー優先度・パイプライン名等は
    ///          こちらを経由して設定する（このコンポーネント自体はそれらの委譲用ラッパーを持たない）
    SpriteRenderer *GetSpriteRenderer() const noexcept { return spriteRenderer_; }
    /// @brief 内部で自動管理しているMeshFilterを取得する（常に"Rect2D"メッシュ固定）
    MeshFilter *GetMeshFilter() const noexcept { return meshFilter_; }

    //==================================================
    // フィットモード
    //==================================================

    /// @brief 描画先の解像度に対する自動フィット方法を設定する
    /// @details None以外を指定した場合、毎フレーム自身のTransform.Scale（X/Yのみ）を
    ///          自動で書き換える。Anchor/Pivotが既定値(0.5,0.5)のままであれば、
    ///          フィット後も表示位置はTransform.Translateを中心に保たれる
    void SetFitMode(FitMode fitMode) noexcept { fitMode_ = fitMode; }
    FitMode GetFitMode() const noexcept { return fitMode_; }

    //==================================================
    // マウス座標変換
    //==================================================

    /// @brief 表示先ウィンドウ上のマウス座標を、ソースScreenBufferの解像度基準のピクセル座標へ変換する
    /// @param outPixel 変換結果（ScreenBufferのピクセル座標。左上原点、X右・Y下方向）
    /// @return 変換に成功し、かつマウスがスプライトの表示範囲内にある場合はtrue
    bool TryGetOffscreenMousePosition(Vector2 &outPixel) const {
        if (!spriteRenderer_) return false;

        auto *windowComponent = ResolveWindow();
        Window *window = windowComponent ? windowComponent->GetWindow() : nullptr;
        if (!window || !Window::IsExist(window)) return false;

        Transform *cameraTransform = nullptr;
        Camera2D *camera2d = ResolveDisplayCamera(cameraTransform);
        if (!camera2d || !cameraTransform) return false;

        auto *source = ResolveSource();
        ScreenBuffer *buffer = source ? source->GetScreenBuffer() : nullptr;
        if (!buffer || !ScreenBuffer::IsExist(buffer)) return false;

        auto *sceneContext = GetOwnerSceneContext();
        Input *input = sceneContext ? sceneContext->GetInput() : nullptr;
        if (!input) return false;

        const float clientWidth = static_cast<float>(window->GetClientWidth());
        const float clientHeight = static_cast<float>(window->GetClientHeight());
        if (clientWidth <= 0.0f || clientHeight <= 0.0f) return false;

        // 1. Windowクライアント座標(px) → NDC（Yは画面下が-1になるよう反転）
        const POINT mousePos = input->GetMouse().GetPos(window);
        const float ndcX = (static_cast<float>(mousePos.x) / clientWidth) * 2.0f - 1.0f;
        const float ndcY = 1.0f - (static_cast<float>(mousePos.y) / clientHeight) * 2.0f;

        // 2. NDC → ワールド座標（表示カメラのビュー射影行列の逆行列）
        Matrix4x4 projection;
        projection.MakeOrthographicMatrix(
            0.0f, 0.0f, camera2d->GetWidth(), camera2d->GetHeight(),
            camera2d->GetNearClip(), camera2d->GetFarClip());
        const Matrix4x4 view = cameraTransform->GetWorldMatrix().Inverse();
        const Matrix4x4 viewProjectionInverse = (view * projection).Inverse();
        const Vector3 worldPos = Vector3(ndcX, ndcY, 0.0f).Transform(viewProjectionInverse);

        // 3. ワールド座標 → スプライトのローカル座標（-0.5～0.5の単位クアッド空間）
        const Matrix4x4 spriteWorldInverse = spriteRenderer_->GetWorldMatrix().Inverse();
        const Vector3 localPos = worldPos.Transform(spriteWorldInverse);

        // 4. ローカル座標 → 正規化座標（(0,0)=左下～(1,1)=右上）。範囲外ならスプライト外
        const float nx = localPos.x + 0.5f;
        const float ny = localPos.y + 0.5f;
        if (nx < 0.0f || nx > 1.0f || ny < 0.0f || ny > 1.0f) return false;

        // 5. 正規化座標 → テクスチャUV（メッシュのV反転規約に合わせる） → ScreenBufferのピクセル座標
        const float u = nx;
        const float v = 1.0f - ny;
        outPixel = Vector2(u * static_cast<float>(buffer->GetWidth()), v * static_cast<float>(buffer->GetHeight()));
        return true;
    }

    /// @brief マウスカーソルが表示範囲内にあるかどうかを取得する
    bool IsMouseOverOffscreen() const {
        Vector2 dummy{};
        return TryGetOffscreenMousePosition(dummy);
    }

protected:
    void Initialize() override {
        EnsureInternalMaterial();
        // 生成はしない（同オブジェクトの他エントリがまだJSONから読み込まれきっていない可能性があり、
        // ここで先に生成すると後から読み込まれる正規のMeshFilter/SpriteRendererと重複してしまうため）。
        // 既に存在するものがあれば拾うだけに留め、実際の生成はUpdate()の初回に遅延する
        auto *objectContext = GetOwnerObjectContext();
        if (objectContext) {
            if (!meshFilter_) meshFilter_ = objectContext->GetComponent<MeshFilter>();
            if (!spriteRenderer_) spriteRenderer_ = objectContext->GetComponent<SpriteRenderer>();
        }

        auto *sceneRenderer = GetOrAddSceneRenderer();
        if (sceneRenderer) {
            sceneRenderer->RegisterScreenBufferViewport(this);
        }
    }

    void Update() override {
        EnsureInternalComponents();
        if (spriteRenderer_ && materialHandle_ != MaterialManager::kInvalidHandle &&
            spriteRenderer_->GetMaterialHandle() != materialHandle_) {
            spriteRenderer_->SetMaterialHandle(materialHandle_);
        }
        SyncMaterialTexture();
        ApplyFitMode();
    }

    void Finalize() override {
        ReleaseInternalMaterial();

        auto *sceneContext = GetOwnerSceneContext();
        auto *sceneRenderer = sceneContext ? sceneContext->GetComponent<SceneRenderer>() : nullptr;
        if (sceneRenderer) {
            sceneRenderer->UnregisterScreenBufferViewport(this);
        }
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        TargetObjectSelector::ShowSelector(TranslationLabel("component.screenbufferviewport.source"), GetOwnerSceneContext(), sourceObjectID_, true, true);
        TargetObjectSelector::ShowSelector(TranslationLabel("component.screenbufferviewport.display_camera"), GetOwnerSceneContext(), displayCameraObjectID_, true, false);

        const char *kFitModeLabels[] = {
            TranslationC("component.screenbufferviewport.fitmode.none"),
            TranslationC("component.screenbufferviewport.fitmode.stretch"),
            TranslationC("component.screenbufferviewport.fitmode.letterbox"),
        };
        int fitModeIndex = static_cast<int>(fitMode_);
        if (ImGui::Combo(TranslationLabel("component.screenbufferviewport.fit_mode"), &fitModeIndex, kFitModeLabels, IM_ARRAYSIZE(kFitModeLabels))) {
            fitMode_ = static_cast<FitMode>(fitModeIndex);
        }

        ImGui::TextDisabled("%s", TranslationC("component.screenbufferviewport.desc_delegate"));

        Vector2 pos{};
        if (TryGetOffscreenMousePosition(pos)) {
            ImGui::Text("Mouse: (%.1f, %.1f)", pos.x, pos.y);
        } else {
            ImGui::TextDisabled("Mouse: -");
        }

        if (fitMode_ != FitMode::None) {
            if (auto *fitTarget = ResolveRenderTarget()) {
                ImGui::Text("Fit Target: %ux%u [%s]", fitTarget->GetRenderTargetWidth(), fitTarget->GetRenderTargetHeight(), fitTarget->GetRenderTargetName().c_str());
            } else {
                ImGui::TextDisabled("Fit Target: - (内部SpriteRendererのTargetが未設定か利用不可)");
            }

            auto *source = ResolveSource();
            ScreenBuffer *buffer = source ? source->GetScreenBuffer() : nullptr;
            if (buffer && ScreenBuffer::IsExist(buffer)) {
                ImGui::Text("Fit Source: %ux%u", buffer->GetWidth(), buffer->GetHeight());
            } else {
                ImGui::TextDisabled("Fit Source: - (Sourceが未設定か利用不可)");
            }

            auto *objectContext = GetOwnerObjectContext();
            auto *transform = objectContext ? objectContext->GetComponent<Transform>() : nullptr;
            if (transform) {
                const Vector3 &scale = transform->GetScale();
                ImGui::Text("Transform Scale: (%.1f, %.1f, %.1f)", scale.x, scale.y, scale.z);
            } else {
                ImGui::TextDisabled("Transform Scale: - (Transformが見つかりません)");
            }
        }
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["sourceObjectID"] = ToJSON(sourceObjectID_);
        json["displayCameraObjectID"] = ToJSON(displayCameraObjectID_);
        json["fitMode"] = static_cast<int>(fitMode_);
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        sourceObjectID_ = json.contains("sourceObjectID") ? FromJSON<UUID128>(json["sourceObjectID"]) : UUID128();
        displayCameraObjectID_ = json.contains("displayCameraObjectID") ? FromJSON<UUID128>(json["displayCameraObjectID"]) : UUID128();
        fitMode_ = static_cast<FitMode>(json.value("fitMode", static_cast<int>(FitMode::None)));
        return true;
    }

private:
    SceneRenderer *GetOrAddSceneRenderer() const {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext) return nullptr;
        auto *sceneRenderer = sceneContext->GetComponent<SceneRenderer>();
        if (!sceneRenderer) {
            sceneRenderer = sceneContext->AddComponent<SceneRenderer>();
        }
        return sceneRenderer;
    }

    ScreenBufferObject *ResolveSource() const {
        auto *obj = GetSourceObject();
        return obj ? obj->GetComponent<ScreenBufferObject>() : nullptr;
    }

    IWindowObjectComponent *ResolveWindow() const {
        if (!spriteRenderer_) return nullptr;
        EmptyObject *targetObj = spriteRenderer_->GetTargetObject();
        if (!targetObj) return nullptr;
        if (auto *w = targetObj->GetComponent<NormalWindowObject>()) return w;
        if (auto *w = targetObj->GetComponent<OverlayWindowObject>()) return w;
        return nullptr;
    }

    Camera2D *ResolveDisplayCamera(Transform *&outTransform) const {
        outTransform = nullptr;
        auto *obj = GetDisplayCameraObject();
        if (!obj) return nullptr;
        outTransform = obj->GetComponent<Transform>();
        return obj->GetComponent<Camera2D>();
    }

    /// @brief 内部SpriteRendererが実際に描画されている先のIRenderTargetを解決する
    /// @details ResolveWindow()と異なりWindow限定ではなく、ScreenBufferへの入れ子表示も許容する。
    ///          対象オブジェクトに描画先コンポーネントが複数付与されている場合は最初に見つかったものを使う
    IRenderTarget *ResolveRenderTarget() const {
        if (!spriteRenderer_) return nullptr;
        EmptyObject *targetObj = spriteRenderer_->GetTargetObject();
        if (!targetObj) return nullptr;
        std::vector<IRenderTarget *> targets;
        SceneRenderer::CollectRenderTargets(targetObj, targets);
        for (auto *target : targets) {
            if (target && target->IsRenderTargetAvailable()) return target;
        }
        return nullptr;
    }

    /// @brief FitModeに応じて、自身のTransform.Scale（X/Yのみ）を描画先の解像度に合わせて書き換える
    /// @details Noneの場合は何もせず、Transform.Scaleの手動設定をそのまま尊重する
    void ApplyFitMode() {
        if (fitMode_ == FitMode::None) return;
        if (!spriteRenderer_) return;

        auto *source = ResolveSource();
        ScreenBuffer *buffer = source ? source->GetScreenBuffer() : nullptr;
        if (!buffer || !ScreenBuffer::IsExist(buffer)) return;
        const float sourceWidth = static_cast<float>(buffer->GetWidth());
        const float sourceHeight = static_cast<float>(buffer->GetHeight());
        if (sourceWidth <= 0.0f || sourceHeight <= 0.0f) return;

        IRenderTarget *target = ResolveRenderTarget();
        if (!target) return;
        const float targetWidth = static_cast<float>(target->GetRenderTargetWidth());
        const float targetHeight = static_cast<float>(target->GetRenderTargetHeight());
        if (targetWidth <= 0.0f || targetHeight <= 0.0f) return;

        auto *objectContext = GetOwnerObjectContext();
        auto *transform = objectContext ? objectContext->GetComponent<Transform>() : nullptr;
        if (!transform) return;

        float displayWidth = targetWidth;
        float displayHeight = targetHeight;
        if (fitMode_ == FitMode::Letterbox) {
            const float fitScale = std::min(targetWidth / sourceWidth, targetHeight / sourceHeight);
            displayWidth = sourceWidth * fitScale;
            displayHeight = sourceHeight * fitScale;
        }

        Vector3 scale = transform->GetScale();
        scale.x = displayWidth;
        scale.y = displayHeight;
        transform->SetScale(scale);
    }

    /// @brief MeshFilter/SpriteRendererを、既存があれば拾い、無ければここで生成する
    /// @details Update()は同オブジェクトの全コンポーネントがJSONから読み込まれきった後にのみ
    ///          呼ばれるため、ここで初めて「本当に無い場合だけ生成する」判定が安全に行える
    void EnsureInternalComponents() {
        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return;

        if (!meshFilter_ || !objectContext->HasComponent(meshFilter_)) {
            meshFilter_ = objectContext->GetComponent<MeshFilter>();
            if (!meshFilter_) meshFilter_ = objectContext->AddComponent<MeshFilter>();
        }
        if (meshFilter_) {
            static const ModelManager::ModelHandle kRect2DHandle =
                ModelManager::GetModelHandleFromAssetPath("PrimitiveMesh-Rect2D");
            if (meshFilter_->GetMeshHandle() != kRect2DHandle) {
                meshFilter_->SetMeshHandle(kRect2DHandle);
            }
        }

        if (!spriteRenderer_ || !objectContext->HasComponent(spriteRenderer_)) {
            spriteRenderer_ = objectContext->GetComponent<SpriteRenderer>();
            if (!spriteRenderer_) spriteRenderer_ = objectContext->AddComponent<SpriteRenderer>();
        }
    }

    void EnsureInternalMaterial() {
        if (materialHandle_ != MaterialManager::kInvalidHandle) return;
        if (internalMaterialName_.empty()) {
            const auto *owner = GetOwnerObject();
            internalMaterialName_ = "__ScreenBufferViewport_" +
                (owner ? owner->GetObjectID().ToString() : std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        }
        materialHandle_ = MaterialManager::GetMaterialHandleFromName(internalMaterialName_);
        if (materialHandle_ == MaterialManager::kInvalidHandle) {
            MaterialManager::Material mat{};
            mat.name = internalMaterialName_;
            mat.enableLighting = false;
            mat.enableShadowMapProjection = false;
            materialHandle_ = MaterialManager::RegisterMaterial(internalMaterialName_, mat);
        }
        lastTextureHandle_ = TextureManager::kInvalidHandle;
    }

    void ReleaseInternalMaterial() {
        if (!internalMaterialName_.empty()) {
            MaterialManager::RemoveMaterial(internalMaterialName_);
        }
        materialHandle_ = MaterialManager::kInvalidHandle;
        lastTextureHandle_ = TextureManager::kInvalidHandle;
    }

    /// @brief 内部マテリアルのテクスチャハンドルを、ソースScreenBufferの現在の登録名に同期する
    void SyncMaterialTexture() {
        if (materialHandle_ == MaterialManager::kInvalidHandle) return;
        auto *source = ResolveSource();
        ScreenBuffer *buffer = source ? source->GetScreenBuffer() : nullptr;
        const auto texHandle = (buffer && ScreenBuffer::IsExist(buffer))
            ? TextureManager::GetTextureFromFileName(source->GetName())
            : TextureManager::kInvalidHandle;
        if (texHandle == lastTextureHandle_) return;
        lastTextureHandle_ = texHandle;
        if (auto *mat = MaterialManager::GetMaterial(materialHandle_)) {
            mat->textureHandle = texHandle;
        }
    }

    UUID128 sourceObjectID_{};
    UUID128 displayCameraObjectID_{};
    FitMode fitMode_ = FitMode::None;

    // 内部専有（非シリアライズ、実行時に自動生成/解決される）
    MeshFilter *meshFilter_ = nullptr;
    SpriteRenderer *spriteRenderer_ = nullptr;
    std::string internalMaterialName_;
    MaterialManager::MaterialHandle materialHandle_ = MaterialManager::kInvalidHandle;
    TextureManager::TextureHandle lastTextureHandle_ = TextureManager::kInvalidHandle;
};

REGISTER_COMPONENT_OBJECT(ScreenBufferViewport)

} // namespace KashipanEngine
