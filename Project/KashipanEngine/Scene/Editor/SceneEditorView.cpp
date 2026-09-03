#include "SceneEditorView.h"
#ifdef USE_IMGUI
#include <ImGuizmo.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>
#include <numbers>
#include <type_traits>
#include <variant>

#include "Core/ProjectPaths.h"
#include "Debug/ScriptDebugDraw.h"
#include "Scene/Editor/EditorSettings.h"
#include "Scene/Editor/EditorWindowChrome.h"
#include "Scene/Editor/PrefabUtility.h"
#include "Scene/Editor/SceneEditorCommands.h"
#include "Scene/Editor/SceneObjectHierarchy.h"
#include "Utilities/Translation.h"
#include "Scene/Components/Render/SceneRenderer.h"
#include "Scene/Components/SceneObjectCollider.h"
#include "Graphics/ScreenBuffer.h"
#include "Graphics/Resources/ConstantBufferResource.h"
#include "Scene/RenderTargetCarryOverRegistry.h"
#include "Objects/EmptyObject.h"
#include "Objects/Components/MeshFilter.h"
#include "Objects/Components/Render/Camera2D.h"
#include "Objects/Components/Render/Camera3D.h"
#include "Objects/Components/Render/CameraRenderer.h"
#include "Objects/Components/Render/SceneViewOrbitState.h"
#include "Objects/Components/Render/SceneViewCameraSettings.h"
#include "Objects/Components/Render/Light.h"
#include "Objects/Components/Render/LightRenderer.h"
#include "Objects/Components/Render/MeshRenderer.h"
#include "Objects/Components/Render/ScreenBufferObject.h"
#include "Objects/Components/Render/SkinnedMeshRenderer.h"
#include "Objects/Components/Render/SpriteRenderer.h"
#include "Objects/Components/Render/TextRenderer.h"
#include "Objects/Components/Render/TilemapRenderer.h"
#include "Objects/Components/Collider/ICollider.h"
#include "Objects/Components/Collider/RayCollider.h"
#include "Objects/Components/Transform.h"
#include "Assets/MaterialManager.h"
#include "Assets/ModelManager.h"
#include "Assets/TextureManager.h"
#include "Math/Quaternion.h"
#include "Utilities/MathUtils.h"
#include "Utilities/AssetDragDropPayload.h"
#include "Utilities/FileIO/JSON.h"

namespace KashipanEngine {

SceneEditorView::SceneEditorView(Passkey<SceneEditor>, SceneEditorContext *context)
    : context_(context), gizmoOperation_(ImGuizmo::TRANSLATE), gizmoMode_(ImGuizmo::LOCAL) {
    // デバッグ表示の有効/無効を復元する（再起動後も維持される）
    showGrid_ = EditorSettings::GetBool("sceneView.showGrid", true);
    showLightMarkers_ = EditorSettings::GetBool("sceneView.showLightMarkers", true);
    showCameraMarkers_ = EditorSettings::GetBool("sceneView.showCameraMarkers", true);
    showColliderGizmos_ = EditorSettings::GetBool("sceneView.showColliderGizmos", true);
    showBoneGizmos_ = EditorSettings::GetBool("sceneView.showBoneGizmos", false);

    // ギズモのグリッドスナップ設定を復元する（再起動後も維持される）
    gizmoSnapEnabled_ = EditorSettings::GetBool("sceneView.gizmoSnapEnabled", false);
    gizmoSnapTranslate_ = EditorSettings::GetFloat("sceneView.gizmoSnapTranslate", 1.0f);
    gizmoSnapRotateDegrees_ = EditorSettings::GetFloat("sceneView.gizmoSnapRotateDegrees", 15.0f);
    gizmoSnapScale_ = EditorSettings::GetFloat("sceneView.gizmoSnapScale", 0.1f);

    // シーンビューの表示モード（2D/3D併用・3Dのみ・2Dのみ）を復元する（再起動後も維持される）
    const std::string displayModeStr = EditorSettings::GetString("sceneView.displayMode", "Combined");
    if (displayModeStr == "ThreeDOnly") displayMode_ = SceneRenderer::EditorDisplayMode::ThreeDOnly;
    else if (displayModeStr == "TwoDOnly") displayMode_ = SceneRenderer::EditorDisplayMode::TwoDOnly;
    else displayMode_ = SceneRenderer::EditorDisplayMode::Combined;

    // 3D/2Dカメラの位置・向き・パン・ズーム等はシーンごとにEditorSettingsへ保存される。
    // ここではまだシーンIDが確定していない（Scene::Sceneのデリゲートコンストラクタ経由で
    // このSceneEditorViewが構築される時点では、JSONからのsceneID_読み込みがまだ行われていない）
    // ため、初回のEnsureCameraSettingsComponentで遅延読み込みする

    // 背景設定を復元する（再起動後も維持される）
    backgroundColor_.x = EditorSettings::GetFloat("sceneView.backgroundColor.r", 0.0f);
    backgroundColor_.y = EditorSettings::GetFloat("sceneView.backgroundColor.g", 0.0f);
    backgroundColor_.z = EditorSettings::GetFloat("sceneView.backgroundColor.b", 0.0f);
    backgroundColor_.w = EditorSettings::GetFloat("sceneView.backgroundColor.a", 1.0f);
    backgroundTexturePath_ = EditorSettings::GetString("sceneView.backgroundTexturePath", std::string{});
}

SceneEditorView::~SceneEditorView() {
    if (context_) {
        if (auto *sceneRenderer = context_->GetComponent<SceneRenderer>()) {
            sceneRenderer->SetEditorTarget(nullptr, nullptr);
            sceneRenderer->SetEditorSelectedObjects({});
            sceneRenderer->SetEditorGhostPreviewMeshes({});
        }
    }
    // screenBuffer_ はこのクラスが直接所有している（ScreenBufferObjectコンポーネントを介さない）。
    // ScreenBufferObject::Finalizeと同様、実際には即座に破棄せず次のシーンの
    // SceneEditorViewへの引き継ぎ候補としてRenderTargetCarryOverRegistryへ預ける
    // （シーン切り替え中でなければ即座に破棄される）
    if (screenBuffer_ && ScreenBuffer::IsExist(screenBuffer_)) {
        RenderTargetCarryOverRegistry::Deposit(RenderTargetCarryOverRegistry::Kind::ScreenBuffer, kScreenBufferName, screenBuffer_,
            [b = screenBuffer_] { if (ScreenBuffer::IsExist(b)) b->DestroyNotify(); });
    }
    screenBuffer_ = nullptr;
    sceneViewObject_ = nullptr;
}

void SceneEditorView::ShowImGui(const std::unordered_set<EmptyObject *> &selectedObjects, SceneEditorCommands *commands, SceneObjectHierarchy *hierarchy) {
    EnsureResources();
    // SceneViewCameraSettingsコンポーネント（Inspectorでの編集結果）を読み取り、
    // このクラスの内部状態（target_/eye_/yaw_/pitch_/...）へ反映する
    PullCameraSettingsFromComponent();
    UpdateCameraBuffer();
    UpdateCamera2DBuffer();
    RegisterEditorTarget();
    UpdateEditorDebugDraw();
    // シーンビュー上で選択中オブジェクトへアウトラインを付ける（このシーンビュー用描画先にのみ適用される）
    if (auto *sceneRenderer = context_->GetComponent<SceneRenderer>()) {
        sceneRenderer->SetEditorSelectedObjects(selectedObjects);
    }
    ShowSceneViewWindow(selectedObjects, commands, hierarchy);
    // マウス操作（HandleCameraInput等）による今フレームの変更をコンポーネント・EditorSettingsへ反映する
    PushCameraSettingsToComponent();
}

void SceneEditorView::EnsureResources() {
    EnsureSceneViewObject();
    EnsureCameraSettingsComponent();
    if (!screenBuffer_ || !ScreenBuffer::IsExist(screenBuffer_)) {
        // シーン切り替え中であれば、直前のSceneEditorViewが預けたバッファを引き継ぐ
        // （引き継げればサイズ・内容がそのまま継続し、チラつきや無駄な再生成を避けられる）
        if (auto *carried = static_cast<ScreenBuffer *>(
                RenderTargetCarryOverRegistry::Claim(RenderTargetCarryOverRegistry::Kind::ScreenBuffer, kScreenBufferName))) {
            screenBuffer_ = carried;
        } else {
            screenBuffer_ = ScreenBuffer::Create(1280, 720, kScreenBufferName);
        }
    }
    if (!cameraBuffer_) {
        cameraBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(CameraConstant));
    }
    if (!camera2DBuffer_) {
        camera2DBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(Camera2DConstant));
    }
}

void SceneEditorView::StripLegacyScreenBufferComponent(EmptyObject *sceneViewObject) {
    if (!sceneViewObject) return;
    if (!sceneViewObject->GetComponent<ScreenBufferObject>()) return;
    // RemoveComponent経由でFinalize()を呼び、旧バッファを正しく後始末（登録解除）させてから取り除く
    sceneViewObject->RemoveComponents<ScreenBufferObject>();
}

void SceneEditorView::EnsureSceneViewObject() {
    sceneViewObject_ = nullptr;
    if (!context_) return;

    // 生ポインタをフレームをまたいでキャッシュせず、毎回UUIDから引き直す
    // （Play開始時のDeleteEditorOnlyObjectsで対象が削除され得るため、CameraRenderer::GetTargetObject
    // と同じ方式でダングリングポインタ参照を避ける）
    if (sceneViewObjectID_.IsValid()) {
        sceneViewObject_ = context_->GetSceneObject(sceneViewObjectID_);
        if (sceneViewObject_) {
            StripLegacyScreenBufferComponent(sceneViewObject_);
            return;
        }
    }

    for (auto *obj : context_->GetSceneObjects()) {
        if (!obj || !obj->IsEditorOnly()) continue;
        // 新方式のSceneViewCameraSettings、または旧バージョンで残っているCamera3D（移行対象）
        // のどちらかを持っていれば「Scene View」オブジェクトとみなす
        if (obj->GetComponent<SceneViewCameraSettings>() || obj->GetComponent<Camera3D>()) {
            sceneViewObject_ = obj;
            break;
        }
    }

    if (sceneViewObject_) {
        sceneViewObjectID_ = sceneViewObject_->GetObjectID();
        // 旧バージョンで保存されたシーンJSONに残っているScreenBufferObjectを取り除く
        // （残したままだと、このクラスが直接生成するscreenBuffer_と同名バッファが二重に
        // 生成されてしまう）
        StripLegacyScreenBufferComponent(sceneViewObject_);
        return;
    }

    // シーン上に見つからない場合は新規作成する。ブートストラップ処理のため、Undo/Redoコマンドは通さない。
    // カメラの実体はこのクラスが直接持つため、ここではTransform（自動付与、未使用）のみを持つ
    // 空オブジェクトを作る。SceneViewCameraSettingsコンポーネントはEnsureCameraSettingsComponent
    // が付与する
    EmptyObject *newObj = context_->CreateEmptyObject("Scene View");
    if (!newObj) return;
    newObj->SetEditorOnly(true);
    sceneViewObject_ = newObj;
    sceneViewObjectID_ = newObj->GetObjectID();
}

void SceneEditorView::EnsureCameraSettingsComponent() {
    if (!sceneViewObject_) return;

    auto *settings = sceneViewObject_->GetComponent<SceneViewCameraSettings>();
    if (!settings) settings = sceneViewObject_->AddComponent<SceneViewCameraSettings>();
    if (!settings) return;

    auto *legacyCamera3d = sceneViewObject_->GetComponent<Camera3D>();
    if (!hasLoadedCameraSettings_) {
        hasLoadedCameraSettings_ = true;
        if (!LoadCameraSettingsFromEditorSettings() && legacyCamera3d) {
            // このシーン用の保存値がまだ無く、旧バージョンのCamera3D/Transformが残っている場合、
            // 直前まで使っていたカメラ位置をリセットせずに引き継ぐ
            MigrateLegacyCameraState(legacyCamera3d);
        }
    }

    // このコンポーネントインスタンスへまだ一度もSceneEditorViewの値を書き込んでいない場合
    // （AddComponentで今フレーム新規生成された、またはPlay開始時のDeleteEditorOnlyObjects・
    // Play終了時のスナップショット復元等でシーンJSONから読み込まれた直後で既定値のまま）、
    // ここで現在の内部状態を書き込んでおかないと、直後のPullCameraSettingsFromComponentで
    // 既定値により内部状態が上書きされてしまう（Play開始・終了時にカメラが原点へ
    // リセットされる不具合の原因だった）
    if (!settings->hasSyncedFromEditorView) {
        WriteCameraStateToComponent(settings);
    }

    // 旧バージョンのコンポーネントは、移行の有無に関わらず必ず取り除く
    // （残しておくとカメラの実体が二重に存在してしまうため）
    if (legacyCamera3d) sceneViewObject_->RemoveComponents<Camera3D>();
    if (sceneViewObject_->GetComponent<SceneViewOrbitState>()) sceneViewObject_->RemoveComponents<SceneViewOrbitState>();
}

void SceneEditorView::MigrateLegacyCameraState(Camera3D *legacyCamera3d) {
    if (!sceneViewObject_ || !legacyCamera3d) return;
    if (auto *transform = sceneViewObject_->GetComponent<Transform>()) {
        const Vector3 forward = transform->GetRotateQuaternion().RotateVector(Vector3(0.0f, 0.0f, 1.0f));
        yaw_ = std::atan2(forward.x, forward.z);
        pitch_ = std::clamp(std::asin(std::clamp(-forward.y, -1.0f, 1.0f)), -1.55f, 1.55f);
        if (auto *orbitState = sceneViewObject_->GetComponent<SceneViewOrbitState>()) {
            distance_ = orbitState->GetDistance();
        }
        eye_ = transform->GetTranslate();
        target_ = eye_ + forward * distance_;
    }
    fovY_ = legacyCamera3d->GetFovY();
    nearClip_ = legacyCamera3d->GetNearClip();
    farClip_ = legacyCamera3d->GetFarClip();
    enableJitter_ = legacyCamera3d->IsJitterEnabled();
}

void SceneEditorView::PullCameraSettingsFromComponent() {
    auto *settings = sceneViewObject_ ? sceneViewObject_->GetComponent<SceneViewCameraSettings>() : nullptr;
    if (!settings) return;
    constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;
    target_ = settings->target;
    eye_ = settings->eye;
    distance_ = settings->distance;
    yaw_ = settings->yawDegrees * kDegToRad;
    pitch_ = std::clamp(settings->pitchDegrees * kDegToRad, -1.55f, 1.55f);
    flyMode_ = settings->flyMode;
    flySpeed_ = settings->flySpeed;
    fovY_ = settings->fovY;
    nearClip_ = settings->nearClip;
    farClip_ = settings->farClip;
    enableJitter_ = settings->enableJitter;
    pan2D_ = settings->pan2D;
    zoom2D_ = settings->zoom2D;
    cameraDistance2D_ = settings->cameraDistance2D;
}

void SceneEditorView::WriteCameraStateToComponent(SceneViewCameraSettings *settings) const {
    if (!settings) return;
    constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>;
    settings->target = target_;
    settings->eye = eye_;
    settings->distance = distance_;
    settings->yawDegrees = yaw_ * kRadToDeg;
    settings->pitchDegrees = pitch_ * kRadToDeg;
    settings->flyMode = flyMode_;
    settings->flySpeed = flySpeed_;
    settings->fovY = fovY_;
    settings->nearClip = nearClip_;
    settings->farClip = farClip_;
    settings->enableJitter = enableJitter_;
    settings->pan2D = pan2D_;
    settings->zoom2D = zoom2D_;
    settings->cameraDistance2D = cameraDistance2D_;
    settings->hasSyncedFromEditorView = true;
}

void SceneEditorView::PushCameraSettingsToComponent() {
    WriteCameraStateToComponent(sceneViewObject_ ? sceneViewObject_->GetComponent<SceneViewCameraSettings>() : nullptr);

    if (!context_) return;
    constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>;
    JSON json;
    json["target"] = ToJSON(target_);
    json["eye"] = ToJSON(eye_);
    json["distance"] = distance_;
    json["yawDegrees"] = yaw_ * kRadToDeg;
    json["pitchDegrees"] = pitch_ * kRadToDeg;
    json["flyMode"] = flyMode_;
    json["flySpeed"] = flySpeed_;
    json["fovY"] = fovY_;
    json["nearClip"] = nearClip_;
    json["farClip"] = farClip_;
    json["enableJitter"] = enableJitter_;
    json["pan2D"] = ToJSON(pan2D_);
    json["zoom2D"] = zoom2D_;
    json["cameraDistance2D"] = cameraDistance2D_;
    // 値が前回保存時から変わっていない場合、EditorSettings::SetJSON内部の比較により
    // 実際のファイル書き込みは発生しない（毎フレーム呼んでも無駄なIOにはならない）
    EditorSettings::SetJSON(BuildCameraSettingsKey(), json);
}

std::string SceneEditorView::BuildCameraSettingsKey() const {
    return context_ ? ("sceneView.camera." + context_->GetSceneID().ToString()) : std::string{};
}

bool SceneEditorView::LoadCameraSettingsFromEditorSettings() {
    if (!context_) return false;
    const JSON stored = EditorSettings::GetJSON(BuildCameraSettingsKey(), JSON());
    if (!stored.is_object() || stored.empty()) return false;

    constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;
    if (stored.contains("target")) target_ = FromJSON<Vector3>(stored["target"]);
    if (stored.contains("eye")) eye_ = FromJSON<Vector3>(stored["eye"]);
    distance_ = stored.value("distance", distance_);
    yaw_ = stored.value("yawDegrees", yaw_ / kDegToRad) * kDegToRad;
    pitch_ = std::clamp(stored.value("pitchDegrees", pitch_ / kDegToRad) * kDegToRad, -1.55f, 1.55f);
    flyMode_ = stored.value("flyMode", flyMode_);
    flySpeed_ = stored.value("flySpeed", flySpeed_);
    fovY_ = stored.value("fovY", fovY_);
    nearClip_ = stored.value("nearClip", nearClip_);
    farClip_ = stored.value("farClip", farClip_);
    enableJitter_ = stored.value("enableJitter", enableJitter_);
    if (stored.contains("pan2D")) pan2D_ = FromJSON<Vector2>(stored["pan2D"]);
    zoom2D_ = stored.value("zoom2D", zoom2D_);
    cameraDistance2D_ = stored.value("cameraDistance2D", cameraDistance2D_);
    return true;
}

void SceneEditorView::UpdateCameraBuffer() {
    if (!cameraBuffer_ || !screenBuffer_ || !sceneViewObject_) return;

    // ピッチ→ヨーの順で回転（行ベクトル規約）
    Matrix4x4 rotateX;
    rotateX.MakeRotateX(pitch_);
    Matrix4x4 rotateY;
    rotateY.MakeRotateY(yaw_);
    const Matrix4x4 rotation = rotateX * rotateY;
    const Vector3 forward(rotation.m[2][0], rotation.m[2][1], rotation.m[2][2]);

    if (flyMode_) {
        // フライモードでは eye_（カメラ自身の位置）が基準。回転すると注視点はその向きの先へ追従する
        target_ = eye_ + forward * distance_;
    } else {
        // オービットモードでは target_（注視点）が基準。回転するとカメラ側が軌道を描くように動く
        eye_ = target_ - forward * distance_;
    }

    // ワールド行列を直接組み立てる（以前はTransformコンポーネント経由で計算していたが、
    // カメラの実体はもうTransformを使わないため、Transform::GetWorldMatrixと同じ計算
    // （回転→平行移動、スケールは常に単位行列扱い）をここで直接行う）
    Matrix4x4 translateMat;
    translateMat.MakeTranslate(eye_);
    const Matrix4x4 world = rotation * translateMat;
    view_ = world.Inverse();

    const float width = static_cast<float>(screenBuffer_->GetWidth());
    const float height = static_cast<float>(screenBuffer_->GetHeight());
    const float aspect = (height > 0.0f) ? (width / height) : (16.0f / 9.0f);
    projection_.MakePerspectiveFovMatrix(fovY_, aspect, nearClip_, farClip_);

    // GPUへアップロードする投影行列にのみジッターを適用する。projection_自体（メンバ変数）は
    // シャドウのカスケードフィッティングやギズモ操作のワールド→スクリーン変換（ScreenToWorld等）
    // で参照されるため、非ジッターのまま保つ
    Matrix4x4 gpuProjection = projection_;
    if (enableJitter_) {
        ApplyCameraJitter(gpuProjection, aspect);
    }

    CameraConstant constant{};
    constant.view = view_;
    constant.projection = gpuProjection;
    constant.viewProjection = view_ * gpuProjection;
    constant.eyePosition = Vector4(eye_.x, eye_.y, eye_.z, 1.0f);
    constant.fov = fovY_;
    cameraEye_ = eye_;

    if (void *mapped = cameraBuffer_->Map()) {
        std::memcpy(mapped, &constant, sizeof(constant));
    }
}

void SceneEditorView::UpdateCamera2DBuffer() {
    if (!camera2DBuffer_ || !screenBuffer_) return;

    // パン位置(pan2D_)を見る正射影カメラ。Z軸方向を向く固定姿勢（SpriteRendererの単位クアッドが
    // 乗るXY平面を正面から見る）で、3Dフリーカメラのyaw_/pitch_等とは完全に独立している。
    // カメラ自身はZ=0ではなく、コンテンツより手前(-cameraDistance2D_)に置く。SpriteRendererの
    // Transformは既定でZ=0のことが多く、もしカメラもZ=0に置くと近クリップ面(near)より
    // 手前になってしまい、ピッキングのレイ（near→farの方向にしか伸びない）が理論上絶対に
    // そのオブジェクトへ到達しない（tが負になり棄却される）。実際、既存シーンのSpriteRendererは
    // Z=0.0で登録されており、この状態でクリック選択が不安定になる根本原因だった。
    // Inspectorから編集可能なため、0以下（near/farが破綻する）にならないようクランプする
    const float cameraDistance = std::max(cameraDistance2D_, 1.0f);
    view2D_ = Matrix4x4::Identity();
    view2D_.m[3][0] = -pan2D_.x;
    view2D_.m[3][1] = -pan2D_.y;
    view2D_.m[3][2] = cameraDistance;

    const float width = static_cast<float>(screenBuffer_->GetWidth());
    const float height = static_cast<float>(screenBuffer_->GetHeight());
    const float aspect = (height > 0.0f) ? (width / height) : (16.0f / 9.0f);
    // Matrix4x4::MakeOrthographicMatrix(left, top, right, bottom, near, far)のtop/bottom引数は、
    // 見た目の上下ではなく数式上の役割（画面下端に対応するY値をtopへ、画面上端に対応するY値を
    // bottomへ渡す）であることに注意。実際のCamera2D(CameraRenderer::UploadCameraConstant)は
    // top=0, bottom=heightを渡しており、Y=0が画面下端・Y=heightが画面上端になる
    // （＝左下(0,0)原点、Y上向きのワールド座標系）。ここでも同じ規約に合わせ、
    // 画面下端に対応する値(-zoom2D_)をtopへ、画面上端に対応する値(+zoom2D_)をbottomへ渡す。
    // near/farはカメラ位置(cameraDistance手前)を基準にした相対値で、Z=0付近を中心に
    // ±cameraDistance程度の余裕を持たせ、多少Zがずれているコンテンツも問題なく拾えるようにする
    projection2D_.MakeOrthographicMatrix(-zoom2D_ * aspect, -zoom2D_, zoom2D_ * aspect, zoom2D_, 0.1f, cameraDistance * 2.0f);

    Camera2DConstant constant{};
    constant.view = view2D_;
    constant.projection = projection2D_;
    constant.viewProjection = view2D_ * projection2D_;
    if (void *mapped = camera2DBuffer_->Map()) {
        std::memcpy(mapped, &constant, sizeof(constant));
    }
}

namespace {
/// @brief Halton(base)列のindex番目の値を[0,1)で返す（カメラジッター用の低差異乱数。
///        Camera3D::HaltonSequenceと同一実装）
float HaltonSequence(std::uint32_t index, std::uint32_t base) noexcept {
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
} // namespace

void SceneEditorView::ApplyCameraJitter(Matrix4x4 &projection, float aspect) {
    // Halton(2,3)列を8点周期で回す。0番目は(0,0)で偏るため1から使う（Camera3D::ApplyProjectionJitter参照）
    constexpr std::uint32_t kJitterPeriod = 8;
    const std::uint32_t index = (jitterIndex_ % kJitterPeriod) + 1;
    ++jitterIndex_;
    const float hx = HaltonSequence(index, 2) - 0.5f;
    const float hy = HaltonSequence(index, 3) - 0.5f;

    constexpr float kJitterReferenceHeight = 1080.0f;
    const float referenceWidth = kJitterReferenceHeight * aspect;
    const float jitterX = hx * (2.0f / referenceWidth);
    const float jitterY = hy * (2.0f / kJitterReferenceHeight);

    // 透視投影ではview空間zがそのままwになる行（3行目）へ加算することで、
    // 透視除算後に深度へ依らず一定のNDCオフセットになる（シーンビューは常に透視投影）
    projection.m[2][0] += jitterX;
    projection.m[2][1] += jitterY;
}

void SceneEditorView::RegisterEditorTarget() {
    if (!context_) return;
    auto *sceneRenderer = context_->GetComponent<SceneRenderer>();
    if (sceneRenderer && screenBuffer_) {
        // シャドウマップのカスケード計算用にエディターカメラの情報も渡す
        SceneRenderer::EditorCameraInfo cameraInfo;
        cameraInfo.viewProjection = view_ * projection_;
        cameraInfo.position = cameraEye_;
        cameraInfo.nearClip = nearClip_;
        cameraInfo.farClip = farClip_;
        cameraInfo.valid = true;
        // オーナーオブジェクトも渡しておくことで、GetTargetOwner経由でこのScreenBufferへの
        // ポストエフェクト適用対象を「Scene View」オブジェクトへ絞り込めるようにする
        // （未指定のままだとRenderPostProcessがオーナー無しとして処理を打ち切ってしまい、
        // シーンビュー上でポストエフェクトが一切適用されなくなる）
        sceneRenderer->SetEditorTarget(screenBuffer_, cameraBuffer_.get(), cameraInfo, sceneViewObject_);
        sceneRenderer->SetEditorDisplayMode(displayMode_);
        sceneRenderer->SetEditorCamera2DBuffer(camera2DBuffer_.get());
    }
}

void SceneEditorView::ShowSceneViewWindow(const std::unordered_set<EmptyObject *> &selectedObjects, SceneEditorCommands *commands, SceneObjectHierarchy *hierarchy) {
    if (!ImGui::Begin(TranslationLabel("editor.sceneview.window"), nullptr, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }
    DrawFloatingWindowChromeButtons();

    // シーンビューにフォーカスがある間も、ヒエラルキーと同じショートカット
    // （Ctrl+C/Ctrl+V/Ctrl+Shift+V/Ctrl+D）で選択中オブジェクトを操作できるようにする
    if (hierarchy) hierarchy->HandleKeyboardShortcuts();

    // TilemapRendererのタイルペイントツールは、選択中オブジェクトが単体でTilemapRendererを
    // 持つ場合のみ有効化できる。対象外になったら自動でトグルを解除する
    TilemapRenderer *paintableTilemap = nullptr;
    EmptyObject *paintableOwner = nullptr;
    if (selectedObjects.size() == 1) {
        EmptyObject *only = *selectedObjects.begin();
        if (auto *tilemap = only ? only->GetComponent<TilemapRenderer>() : nullptr) {
            paintableTilemap = tilemap;
            paintableOwner = only;
        }
    }
    if (!paintableTilemap) tilemapPaintActive_ = false;

    //--------- ツールバー（項目数が増えてきたため、メニューバーでまとめて表示する） ---------//
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu(TranslationLabel("editor.sceneview.tab.gizmo"))) {
            // ギズモ操作（Translate/Rotate/Scale）とギズモモード（Local/World）の切り替え
            if (ImGui::RadioButton(TranslationLabel("editor.sceneview.gizmo.translate"), gizmoOperation_ == ImGuizmo::TRANSLATE)) gizmoOperation_ = ImGuizmo::TRANSLATE;
            ImGui::SameLine();
            if (ImGui::RadioButton(TranslationLabel("editor.sceneview.gizmo.rotate"), gizmoOperation_ == ImGuizmo::ROTATE)) gizmoOperation_ = ImGuizmo::ROTATE;
            ImGui::SameLine();
            if (ImGui::RadioButton(TranslationLabel("editor.sceneview.gizmo.scale"), gizmoOperation_ == ImGuizmo::SCALE)) gizmoOperation_ = ImGuizmo::SCALE;
            ImGui::SameLine();
            if (ImGui::RadioButton(TranslationLabel("editor.sceneview.gizmo.local"), gizmoMode_ == ImGuizmo::LOCAL)) gizmoMode_ = ImGuizmo::LOCAL;
            ImGui::SameLine();
            if (ImGui::RadioButton(TranslationLabel("editor.sceneview.gizmo.world"), gizmoMode_ == ImGuizmo::WORLD)) gizmoMode_ = ImGuizmo::WORLD;

            // グリッドスナップ（オブジェクトのグリッド配置）設定
            ImGui::Separator();
            if (ImGui::Checkbox(TranslationLabel("editor.sceneview.gizmo.snap"), &gizmoSnapEnabled_)) {
                EditorSettings::SetBool("sceneView.gizmoSnapEnabled", gizmoSnapEnabled_);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TranslationC("editor.sceneview.gizmo.snap.tooltip"));
            ImGui::SetNextItemWidth(80.0f);
            if (ImGui::DragFloat(TranslationLabel("editor.sceneview.gizmo.snap.translate"), &gizmoSnapTranslate_, 0.01f, 0.001f, 1000.0f)) {
                gizmoSnapTranslate_ = std::max(gizmoSnapTranslate_, 0.001f);
                EditorSettings::SetFloat("sceneView.gizmoSnapTranslate", gizmoSnapTranslate_);
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            if (ImGui::DragFloat(TranslationLabel("editor.sceneview.gizmo.snap.rotate"), &gizmoSnapRotateDegrees_, 0.1f, 0.001f, 360.0f)) {
                gizmoSnapRotateDegrees_ = std::max(gizmoSnapRotateDegrees_, 0.001f);
                EditorSettings::SetFloat("sceneView.gizmoSnapRotateDegrees", gizmoSnapRotateDegrees_);
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            if (ImGui::DragFloat(TranslationLabel("editor.sceneview.gizmo.snap.scale"), &gizmoSnapScale_, 0.001f, 0.001f, 1000.0f)) {
                gizmoSnapScale_ = std::max(gizmoSnapScale_, 0.001f);
                EditorSettings::SetFloat("sceneView.gizmoSnapScale", gizmoSnapScale_);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(TranslationLabel("editor.sceneview.tab.camera"))) {
            // カメラ操作モード（オービット/フライ）の切り替え
            if (ImGui::RadioButton(TranslationLabel("editor.sceneview.camera.orbit"), !flyMode_)) {
                flyMode_ = false;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TranslationC("editor.sceneview.camera.orbit.tooltip"));
            ImGui::SameLine();
            if (ImGui::RadioButton(TranslationLabel("editor.sceneview.camera.fly"), flyMode_)) {
                flyMode_ = true;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TranslationC("editor.sceneview.camera.fly.tooltip"));

            // シーンビューの表示モード切り替え（2D/3D併用・3Dのみ・2Dのみ）
            if (ImGui::RadioButton(TranslationLabel("editor.sceneview.display.combined"), displayMode_ == SceneRenderer::EditorDisplayMode::Combined)) {
                displayMode_ = SceneRenderer::EditorDisplayMode::Combined;
                EditorSettings::SetString("sceneView.displayMode", "Combined");
            }
            ImGui::SameLine();
            if (ImGui::RadioButton(TranslationLabel("editor.sceneview.display.threed"), displayMode_ == SceneRenderer::EditorDisplayMode::ThreeDOnly)) {
                displayMode_ = SceneRenderer::EditorDisplayMode::ThreeDOnly;
                EditorSettings::SetString("sceneView.displayMode", "ThreeDOnly");
            }
            ImGui::SameLine();
            if (ImGui::RadioButton(TranslationLabel("editor.sceneview.display.twod"), displayMode_ == SceneRenderer::EditorDisplayMode::TwoDOnly)) {
                displayMode_ = SceneRenderer::EditorDisplayMode::TwoDOnly;
                EditorSettings::SetString("sceneView.displayMode", "TwoDOnly");
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TranslationC("editor.sceneview.display.tooltip"));
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(TranslationLabel("editor.sceneview.tab.display"))) {
            // デバッグ表示の有効/無効切り替え
            if (ImGui::Checkbox(TranslationLabel("editor.sceneview.show.grid"), &showGrid_)) EditorSettings::SetBool("sceneView.showGrid", showGrid_);
            ImGui::SameLine();
            if (ImGui::Checkbox(TranslationLabel("editor.sceneview.show.lights"), &showLightMarkers_)) EditorSettings::SetBool("sceneView.showLightMarkers", showLightMarkers_);
            ImGui::SameLine();
            if (ImGui::Checkbox(TranslationLabel("editor.sceneview.show.cameras"), &showCameraMarkers_)) EditorSettings::SetBool("sceneView.showCameraMarkers", showCameraMarkers_);
            ImGui::SameLine();
            if (ImGui::Checkbox(TranslationLabel("editor.sceneview.show.colliders"), &showColliderGizmos_)) EditorSettings::SetBool("sceneView.showColliderGizmos", showColliderGizmos_);
            ImGui::SameLine();
            if (ImGui::Checkbox(TranslationLabel("editor.sceneview.show.bones"), &showBoneGizmos_)) EditorSettings::SetBool("sceneView.showBoneGizmos", showBoneGizmos_);

            // 背景設定（単色 or テクスチャ）
            if (ImGui::ColorEdit4(TranslationLabel("editor.sceneview.backgroundcolor"), &backgroundColor_.x)) {
                EditorSettings::SetFloat("sceneView.backgroundColor.r", backgroundColor_.x);
                EditorSettings::SetFloat("sceneView.backgroundColor.g", backgroundColor_.y);
                EditorSettings::SetFloat("sceneView.backgroundColor.b", backgroundColor_.z);
                EditorSettings::SetFloat("sceneView.backgroundColor.a", backgroundColor_.w);
            }
            ImGui::SameLine();
            std::vector<std::string> texturePaths;
            for (const auto &entry : TextureManager::GetLoadedTextureListEntries()) {
                texturePaths.push_back(entry.assetPath);
            }
            if (ImGuiCustom::SelectString(TranslationLabel("editor.sceneview.backgroundtexture"), backgroundTexturePath_, texturePaths, true)) {
                EditorSettings::SetString("sceneView.backgroundTexturePath", backgroundTexturePath_);
            }
            // Assetsウィンドウからのテクスチャファイルドラッグ&ドロップも受け付ける
            if (std::string droppedPath; AcceptAssetDragDropTarget(kTextureAssetDragDropType, droppedPath)) {
                backgroundTexturePath_ = droppedPath;
                EditorSettings::SetString("sceneView.backgroundTexturePath", backgroundTexturePath_);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(TranslationLabel("editor.sceneview.tab.tilepaint"))) {
            ShowTilemapPaintToolbar(paintableTilemap);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    //--------- シーンビュー画像 ---------//
    // GetSrvHandle()（現在の読み取り面）ではなく GetPreviewSrvHandle()（このフレームの描画・
    // ポストエフェクト適用が完了した時点で確定した面）を使う。ポストエフェクトのNextPass()呼び出しは
    // 1フレーム中に書き込み面インデックスを複数回進めるため、Update側（このShowImGui）で
    // GetSrvHandle()を読むと、この後のDraw側の処理でちょうど書き換え・作り直しの対象になる面を
    // 参照してしまうことがある（詳細はScreenBuffer::GetPreviewSrvHandle参照。
    // ScreenBufferObject::ShowViewerWindowと同じ理由で同じ対策を行う）
    if (!screenBuffer_ || screenBuffer_->GetPreviewSrvHandle().ptr == 0) {
        ImGui::TextUnformatted(TranslationC("editor.sceneview.notready"));
        ImGui::End();
        return;
    }

    // ウィンドウの表示領域に合わせて ScreenBuffer 自体をリサイズする（Unity等のシーンビューと同じ挙動）
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x >= 1.0f && avail.y >= 1.0f) {
        const auto newWidth = static_cast<std::uint32_t>(avail.x);
        const auto newHeight = static_cast<std::uint32_t>(avail.y);
        if (newWidth != screenBuffer_->GetWidth() || newHeight != screenBuffer_->GetHeight()) {
            screenBuffer_->Resize(newWidth, newHeight);
        }
    }
    const ImVec2 drawSize = avail;
    const ImVec2 imagePos = ImGui::GetCursorScreenPos();
    ImGui::Image(static_cast<ImTextureID>(screenBuffer_->GetPreviewSrvHandle().ptr), drawSize);
    const bool isSceneViewHovered = ImGui::IsItemHovered();

    // Assetsウィンドウからのプレハブファイル（.prefab）のドラッグ&ドロップ。ドラッグ中は毎フレーム
    // カーソル直下の配置予定位置（シーン上のメッシュ表面、無ければ地面平面）へ半透明のプレビューを
    // 表示し、Unity等と同様、実際にドロップされた瞬間にそのままシーンへ配置する
    HandlePrefabDragDrop(hierarchy, imagePos, drawSize);

    //--------- カメラ操作（画像上でのマウス操作） ---------//
    HandleCameraInput();

    //--------- ギズモ操作切り替えのショートカットキー（W:移動 / E:回転 / R:拡縮） ---------//
    HandleGizmoShortcuts(isSceneViewHovered);

    //--------- タイルペイントツール（有効時はクリックが常にペイントになるため、通常の
    //          オブジェクトピッキング・ギズモ操作は行わない） ---------//
    const bool tilemapPaintConsumedInput = tilemapPaintActive_ && paintableTilemap
        && HandleTilemapPaint(paintableOwner, paintableTilemap, commands, imagePos, drawSize);

    //--------- クリックによるオブジェクト選択 ---------//
    if (!tilemapPaintConsumedInput) HandleObjectPicking(hierarchy, imagePos, drawSize);

    // グリッド線・当たり判定のワイヤーフレームは screenBuffer_ へGPUで直接描画される
    // （UpdateEditorDebugDraw で設定済み。DebugGrid/DebugLinesパイプライン参照）

    //--------- 2Dモード専用のグリッド線（3Dモードのグリッドとは別実装のImGuiオーバーレイ） ---------//
    if (showGrid_ && displayMode_ == SceneRenderer::EditorDisplayMode::TwoDOnly) DrawGrid2D(imagePos, drawSize);

    //--------- 2Dモード専用の2Dコライダー可視化（3DのGPUデバッグラインとは別実装のImGuiオーバーレイ） ---------//
    if (showColliderGizmos_ && displayMode_ == SceneRenderer::EditorDisplayMode::TwoDOnly) DrawCollider2DOverlay(imagePos, drawSize);

    //--------- 2Dモード専用のCamera2D表示範囲（3D/2D3DモードのAppendCameraFrustumLinesと対になる表示） ---------//
    if (showCameraMarkers_ && displayMode_ == SceneRenderer::EditorDisplayMode::TwoDOnly) DrawCamera2DBoundsOverlay(imagePos, drawSize);

    //--------- ライトのデバッグ表示（2Dモードでは3D専用のアイコンのため表示しない） ---------//
    if (showLightMarkers_ && displayMode_ != SceneRenderer::EditorDisplayMode::TwoDOnly) DrawLightMarkers(imagePos, drawSize);

    //--------- カメラのデバッグ表示（2Dモードでは3D専用のアイコンのため表示しない） ---------//
    // 2D/3Dどちらのカメラアイコンを出すかはDrawCameraMarkers内部で表示モードに応じて絞り込む
    if (showCameraMarkers_) DrawCameraMarkers(imagePos, drawSize);

    //--------- ImGuizmo によるギズモ表示（タイルペイント中は表示・操作しない） ---------//
    if (!(tilemapPaintActive_ && paintableTilemap)) ShowGizmo(selectedObjects, commands, imagePos, drawSize);

    ImGui::End();
}

void SceneEditorView::HandlePrefabDragDrop(SceneObjectHierarchy *hierarchy, const ImVec2 &imagePos, const ImVec2 &imageSize) {
    bool isHoveringThisView = false;
    if (ImGui::BeginDragDropTarget()) {
        // AcceptPeekOnly = AcceptBeforeDelivery | AcceptNoDrawDefaultRect。ドロップ前でも
        // ペイロードを覗き見できるようにし（IsDelivery()で実際のドロップかを判別する）、
        // 既定のハイライト矩形は出さない（プレビューメッシュ自体が視覚的なフィードバックになるため）
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(kPrefabAssetDragDropType, ImGuiDragDropFlags_AcceptPeekOnly)) {
            IM_ASSERT(payload->DataSize == sizeof(AssetDragDropPayload));
            const auto *assetPayload = static_cast<const AssetDragDropPayload *>(payload->Data);
            const std::string hoveredPath = assetPayload->assetPath;
            isHoveringThisView = true;

            if (payload->IsDelivery()) {
                // 実際にドロップされた瞬間：プレビューと同じ位置計算でシーンへ配置する
                if (hierarchy) {
                    const Vector3 dropPosition = ComputeCursorWorldPosition(ImGui::GetMousePos(), imagePos, imageSize);
                    hierarchy->InstantiatePrefabFile(hoveredPath, nullptr, &dropPosition);
                }
            } else {
                // ドラッグ中（未ドロップ）：カーソル直下の配置予定位置にプレビューメッシュを表示する
                UpdateGhostPreview(hoveredPath, imagePos, imageSize);
            }
        }
        ImGui::EndDragDropTarget();
    }
    // このシーンビュー上でのプレハブドラッグが終わった（ドロップ/キャンセル/範囲外へ移動）場合はプレビューを消す
    if (!isHoveringThisView) {
        ClearGhostPreview();
    }
}

void SceneEditorView::UpdateGhostPreview(const std::string &prefabPath, const ImVec2 &imagePos, const ImVec2 &imageSize) {
    if (!context_) return;
    auto *sceneRenderer = context_->GetComponent<SceneRenderer>();
    if (!sceneRenderer) return;

    // プレハブのパスが変わった時（ドラッグ開始・別アセットへの持ち替え）のみディスクから読み直す。
    // ドラッグ中は毎フレームこの関数が呼ばれるため、ここを都度パースするとホバー中に無駄なIOが走る
    if (!ghostPreviewActive_ || prefabPath != ghostPreviewAssetPath_) {
        ghostPreviewAssetPath_ = prefabPath;
        ghostPreviewNodes_.clear();
        const JSON prefabJson = LoadJSON(ProjectPaths::ToPhysical(prefabPath));
        if (prefabJson.is_object()) {
            ghostPreviewNodes_ = PrefabUtility::LoadPrefabNodes(prefabJson);
        }
        ghostPreviewActive_ = true;
    }

    const Vector3 dropPosition = ComputeCursorWorldPosition(ImGui::GetMousePos(), imagePos, imageSize);
    const auto ghostMeshes = PrefabUtility::BuildGhostPreviewMeshes(ghostPreviewNodes_, dropPosition);

    std::vector<SceneRenderer::GhostPreviewMesh> rendererMeshes;
    rendererMeshes.reserve(ghostMeshes.size());
    for (const auto &mesh : ghostMeshes) {
        rendererMeshes.push_back({ mesh.meshHandle, mesh.worldMatrix });
    }
    sceneRenderer->SetEditorGhostPreviewMeshes(std::move(rendererMeshes));
}

void SceneEditorView::ClearGhostPreview() {
    if (!ghostPreviewActive_) return;
    ghostPreviewActive_ = false;
    ghostPreviewAssetPath_.clear();
    ghostPreviewNodes_.clear();
    if (context_) {
        if (auto *sceneRenderer = context_->GetComponent<SceneRenderer>()) {
            sceneRenderer->SetEditorGhostPreviewMeshes({});
        }
    }
}

void SceneEditorView::HandleCameraInput() {
    if (!ImGui::IsItemHovered()) return;

    // 「2D」表示モード中は3Dフリーカメラ（オービット/フライ）ではなく、専用のパン・ズーム操作を行う
    if (displayMode_ == SceneRenderer::EditorDisplayMode::TwoDOnly) {
        HandleCamera2DInput();
        return;
    }

    ImGuiIO &io = ImGui::GetIO();

    if (flyMode_) {
        // 右ドラッグでその場で見回す（eye_は変えず向きだけ変える。target_は次のUpdateCameraBufferで
        // 新しい向きの先へ追従して再計算される）
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            yaw_ += io.MouseDelta.x * 0.005f;
            pitch_ += io.MouseDelta.y * 0.005f;
            pitch_ = std::clamp(pitch_, -1.55f, 1.55f);
        }
        // 右ボタンを押している間、WASD（+QE上下）で見ている方向基準に移動する
        // （Unityのシーンビューと同じ操作感。Shiftで加速）
        if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
            Matrix4x4 rotateX;
            rotateX.MakeRotateX(pitch_);
            Matrix4x4 rotateY;
            rotateY.MakeRotateY(yaw_);
            const Matrix4x4 rotation = rotateX * rotateY;
            const Vector3 forward(rotation.m[2][0], rotation.m[2][1], rotation.m[2][2]);
            const Vector3 right(rotation.m[0][0], rotation.m[0][1], rotation.m[0][2]);
            const Vector3 up(rotation.m[1][0], rotation.m[1][1], rotation.m[1][2]);

            Vector3 move{ 0.0f, 0.0f, 0.0f };
            if (ImGui::IsKeyDown(ImGuiKey_W)) move = move + forward;
            if (ImGui::IsKeyDown(ImGuiKey_S)) move = move - forward;
            if (ImGui::IsKeyDown(ImGuiKey_D)) move = move + right;
            if (ImGui::IsKeyDown(ImGuiKey_A)) move = move - right;
            if (ImGui::IsKeyDown(ImGuiKey_E)) move = move + up;
            if (ImGui::IsKeyDown(ImGuiKey_Q)) move = move - up;

            const float lengthSq = move.LengthSquared();
            if (lengthSq > 1e-8f) {
                const float speed = (ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift)) ? flySpeed_ * 3.0f : flySpeed_;
                eye_ = eye_ + move.Normalize() * (speed * io.DeltaTime);
            }
        }
        // ホイールで移動速度を調整する
        if (io.MouseWheel != 0.0f) {
            flySpeed_ *= std::pow(1.1f, io.MouseWheel);
            flySpeed_ = std::clamp(flySpeed_, 0.1f, 1000.0f);
        }
    } else {
        // ホイールでズーム
        if (io.MouseWheel != 0.0f) {
            distance_ *= std::pow(0.9f, io.MouseWheel);
            distance_ = std::clamp(distance_, 0.1f, 10000.0f);
        }
        // 右ドラッグで注視点を中心に軌道回転
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            yaw_ += io.MouseDelta.x * 0.005f;
            pitch_ += io.MouseDelta.y * 0.005f;
            pitch_ = std::clamp(pitch_, -1.55f, 1.55f);
        }
        // 中ドラッグでパン
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            Matrix4x4 rotateX;
            rotateX.MakeRotateX(pitch_);
            Matrix4x4 rotateY;
            rotateY.MakeRotateY(yaw_);
            const Matrix4x4 rotation = rotateX * rotateY;
            const Vector3 right(rotation.m[0][0], rotation.m[0][1], rotation.m[0][2]);
            const Vector3 up(rotation.m[1][0], rotation.m[1][1], rotation.m[1][2]);
            const float panSpeed = distance_ * 0.002f;
            target_ = target_ - right * (io.MouseDelta.x * panSpeed) + up * (io.MouseDelta.y * panSpeed);
        }
    }
}

void SceneEditorView::HandleGizmoShortcuts(bool isSceneViewHovered) {
    if (!isSceneViewHovered) return;
    // フライモード中は右ボタン押下中にW/E/QをカメラのXZ/上下移動として使っているため、
    // 右ボタンを押している間はギズモ切り替えショートカットを無効にする
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) return;

    if (ImGui::IsKeyPressed(ImGuiKey_W, false)) gizmoOperation_ = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(ImGuiKey_E, false)) gizmoOperation_ = ImGuizmo::ROTATE;
    if (ImGui::IsKeyPressed(ImGuiKey_R, false)) gizmoOperation_ = ImGuizmo::SCALE;
}

void SceneEditorView::HandleCamera2DInput() {
    ImGuiIO &io = ImGui::GetIO();

    // ホイールでズーム（3Dフリーカメラのdistance_ズームと同じ減衰率）
    if (io.MouseWheel != 0.0f) {
        zoom2D_ *= std::pow(0.9f, io.MouseWheel);
        zoom2D_ = std::clamp(zoom2D_, 0.05f, 10000.0f);
    }
    // 左ドラッグ・中ドラッグどちらでもパンする（3D空間の当たり判定を考える必要が無い2D専用モードのため、
    // 左ドラッグをオブジェクト選択と競合させないよう、選択はクリック＝ドラッグなしの場合のみ反応する
    // HandleObjectPicking側の既存挙動にそのまま委ねる）
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) ||
        (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && io.KeyAlt)) {
        // 画面ピクセル→ワールド単位の換算率（正射影の縦幅 zoom2D_*2 が screenBuffer_ の高さピクセルに対応する）
        const float heightPx = screenBuffer_ ? static_cast<float>(screenBuffer_->GetHeight()) : 0.0f;
        if (heightPx > 0.0f) {
            const float worldPerPixel = (zoom2D_ * 2.0f) / heightPx;
            pan2D_.x -= io.MouseDelta.x * worldPerPixel;
            pan2D_.y += io.MouseDelta.y * worldPerPixel;
        }
    }
}

void SceneEditorView::UpdateEditorDebugDraw() {
    if (!context_) return;
    auto *sceneRenderer = context_->GetComponent<SceneRenderer>();
    if (!sceneRenderer) return;

    EditorDebugDrawSettings settings;
    // EditorDebugDrawSettings::showGridの既定値はtrueのため、2Dモードでは明示的にfalseへ
    // 上書きする（下のif内で条件付きにshowGrid_を代入するだけだと、2Dモードで代入自体を
    // スキップした際に既定値trueのまま送られてしまい、グリッドが消えないバグになる）
    settings.showGrid = false;
    settings.backgroundColor = backgroundColor_;
    settings.backgroundTextureHandle = backgroundTexturePath_.empty()
        ? TextureManager::kInvalidHandle
        : TextureManager::GetTextureFromAssetPath(backgroundTexturePath_);

    // グリッド・カメラ視錐台・ライト方向・ボーンはいずれも3Dフリーカメラ（gCamera3D）の投影で
    // 描かれるGPUデバッグライン。2Dモードでは実際の描画に2D専用の正射影カメラ（gCamera2D）が
    // 使われるため、これらを出したままにすると3Dカメラ視点のまま取り残された残像のように見えてしまう。
    // そのため2Dモード中はここで一切追加しない（描画自体を行わない）。コライダーのみ、2D形状は
    // ImGuiオーバーレイ（DrawCollider2DOverlay）で別途表示するため対象外
    if (displayMode_ != SceneRenderer::EditorDisplayMode::TwoDOnly) {
        settings.showGrid = showGrid_;
        // カメラのズーム量に応じてグリッドの表示範囲を追従させる（Blenderのように「無限」に感じられる範囲を保つ）
        settings.gridFadeDistance = std::clamp(distance_ * 4.0f, 20.0f, 2000.0f);

        if (showColliderGizmos_) {
            AppendColliderDebugLines(settings.lines);
        }
        if (showCameraMarkers_) {
            AppendCameraFrustumLines(settings.lines);
        }
        if (showLightMarkers_) {
            // ライトの向きはImGuiのオーバーレイではなく、エンジン側のデバッグライン描画で行う
            // （オブジェクトとの前後関係が正しく表現される）
            AppendLightDirectionLines(settings.lines);
        }
        if (showBoneGizmos_) {
            // ボーンはモデル内部にあっても姿勢を確認できるよう、深度テストを行わない線として描画する。
            AppendSkeletonBoneLines(settings.overlayLines);
        }

        // スクリプト（Debug::DrawLine）から蓄積されたデバッグ線を合流させる。3Dフリーカメラ
        // （gCamera3D）の投影で描かれるため、2Dモードでは他のデバッグ線と同様に表示しない
        for (const auto &line : ScriptDebugDraw::GetLines()) {
            settings.lines.push_back(DebugLineVertex{ line.start, line.color });
            settings.lines.push_back(DebugLineVertex{ line.end, line.color });
        }
    }
    // 2Dモード中に蓄積され続けないよう、表示するかどうかに関わらず毎フレームクリアする
    ScriptDebugDraw::Clear();

    sceneRenderer->SetEditorDebugDraw(std::move(settings));
}

bool SceneEditorView::ProjectToImage(const Vector3 &worldPosition, const ImVec2 &imagePos, const ImVec2 &imageSize, ImVec2 &outScreenPos, bool clampToVisibleArea) const {
    const Matrix4x4 viewProjection = GetActiveView() * GetActiveProjection();
    const float x = worldPosition.x * viewProjection.m[0][0] + worldPosition.y * viewProjection.m[1][0] + worldPosition.z * viewProjection.m[2][0] + viewProjection.m[3][0];
    const float y = worldPosition.x * viewProjection.m[0][1] + worldPosition.y * viewProjection.m[1][1] + worldPosition.z * viewProjection.m[2][1] + viewProjection.m[3][1];
    const float w = worldPosition.x * viewProjection.m[0][3] + worldPosition.y * viewProjection.m[1][3] + worldPosition.z * viewProjection.m[2][3] + viewProjection.m[3][3];
    if (w <= 1e-4f) return false;
    const float ndcX = x / w;
    const float ndcY = y / w;
    if (clampToVisibleArea && (ndcX < -1.2f || ndcX > 1.2f || ndcY < -1.2f || ndcY > 1.2f)) return false;
    outScreenPos = ImVec2(
        imagePos.x + (ndcX * 0.5f + 0.5f) * imageSize.x,
        imagePos.y + (-ndcY * 0.5f + 0.5f) * imageSize.y);
    return true;
}

void SceneEditorView::DrawLightMarkers(const ImVec2 &imagePos, const ImVec2 &imageSize) {
    if (!context_ || imageSize.x <= 0.0f || imageSize.y <= 0.0f) return;
    auto *sceneRenderer = context_->GetComponent<SceneRenderer>();
    if (!sceneRenderer) return;

    auto *drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(imagePos, ImVec2(imagePos.x + imageSize.x, imagePos.y + imageSize.y), true);

    for (auto *lightRenderer : sceneRenderer->GetLightRenderers()) {
        if (!lightRenderer || !lightRenderer->IsActive()) continue;

        const Vector3 position = lightRenderer->GetWorldPosition();
        ImVec2 center;
        if (!ProjectToImage(position, imagePos, imageSize, center)) continue;

        // ライトの色をアイコン色にする（視認性のため不透明にする）
        ImU32 color = IM_COL32(255, 220, 96, 255);
        auto *light = lightRenderer->GetLight();
        if (light) {
            const Vector4 &lightColor = light->GetColor();
            color = IM_COL32(
                static_cast<int>(std::clamp(lightColor.x, 0.0f, 1.0f) * 255.0f),
                static_cast<int>(std::clamp(lightColor.y, 0.0f, 1.0f) * 255.0f),
                static_cast<int>(std::clamp(lightColor.z, 0.0f, 1.0f) * 255.0f),
                255);
        }

        // 電球風アイコン（円＋放射線）
        constexpr float kRadius = 7.0f;
        drawList->AddCircle(center, kRadius, color, 12, 2.0f);
        drawList->AddCircleFilled(center, 2.5f, color);
        const auto lightType = light ? light->GetType() : Light::Type::Directional;
        if (lightType != Light::Type::Spot) {
            for (int i = 0; i < 8; ++i) {
                const float angle = static_cast<float>(i) * (std::numbers::pi_v<float> / 4.0f);
                const float dirX = std::cos(angle);
                const float dirY = std::sin(angle);
                drawList->AddLine(
                    ImVec2(center.x + dirX * (kRadius + 2.0f), center.y + dirY * (kRadius + 2.0f)),
                    ImVec2(center.x + dirX * (kRadius + 6.0f), center.y + dirY * (kRadius + 6.0f)),
                    color, 1.5f);
            }
        }

        // ライトの向きの線はImGuiではなくエンジン側のデバッグライン描画で行う
        // （AppendLightDirectionLines参照。ここではアイコンのみ描画する）
    }

    drawList->PopClipRect();
}

namespace {
/// @brief NDC座標をワールド座標へ逆投影する（行ベクトル規約、D3Dの深度レンジ[0,1]）
Vector3 UnprojectNdc(const Matrix4x4 &invViewProjection, float ndcX, float ndcY, float ndcZ) {
    const float x = ndcX * invViewProjection.m[0][0] + ndcY * invViewProjection.m[1][0] + ndcZ * invViewProjection.m[2][0] + invViewProjection.m[3][0];
    const float y = ndcX * invViewProjection.m[0][1] + ndcY * invViewProjection.m[1][1] + ndcZ * invViewProjection.m[2][1] + invViewProjection.m[3][1];
    const float z = ndcX * invViewProjection.m[0][2] + ndcY * invViewProjection.m[1][2] + ndcZ * invViewProjection.m[2][2] + invViewProjection.m[3][2];
    float w = ndcX * invViewProjection.m[0][3] + ndcY * invViewProjection.m[1][3] + ndcZ * invViewProjection.m[2][3] + invViewProjection.m[3][3];
    if (std::abs(w) < 1e-6f) w = 1e-6f;
    return Vector3(x / w, y / w, z / w);
}

/// @brief 点を行列で変換する（行ベクトル規約、w除算あり）
Vector3 TransformPoint(const Vector3 &p, const Matrix4x4 &m) {
    const float x = p.x * m.m[0][0] + p.y * m.m[1][0] + p.z * m.m[2][0] + m.m[3][0];
    const float y = p.x * m.m[0][1] + p.y * m.m[1][1] + p.z * m.m[2][1] + m.m[3][1];
    const float z = p.x * m.m[0][2] + p.y * m.m[1][2] + p.z * m.m[2][2] + m.m[3][2];
    float w = p.x * m.m[0][3] + p.y * m.m[1][3] + p.z * m.m[2][3] + m.m[3][3];
    if (std::abs(w) < 1e-8f) w = 1e-8f;
    return Vector3(x / w, y / w, z / w);
}

/// @brief (x0,y0)から(x1,y1)までの格子セルをBresenhamのアルゴリズムで列挙し、visitへ順に渡す
/// @details タイルペイントのドラッグ中、1フレームでマウスが複数セル分動いた場合に塗り残しの
///          隙間ができないよう、前フレームのセルから今フレームのセルまでを線で補間するために使う
template <typename Visitor>
void WalkGridLine(int x0, int y0, int x1, int y1, Visitor &&visit) {
    const int dx = std::abs(x1 - x0);
    const int dy = -std::abs(y1 - y0);
    const int sx = (x0 < x1) ? 1 : -1;
    const int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    int x = x0;
    int y = y0;
    while (true) {
        visit(x, y);
        if (x == x1 && y == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x += sx; }
        if (e2 <= dx) { err += dx; y += sy; }
    }
}

/// @brief 線分（origin + dir*t, t∈[0,1]）と三角形の交差判定（Möller–Trumbore法、両面判定）
/// @param outT 交差した場合、線分上のパラメータt（0=始点、1=終点）
bool SegmentIntersectsTriangle(const Vector3 &origin, const Vector3 &dir,
    const Vector3 &v0, const Vector3 &v1, const Vector3 &v2, float &outT) {
    constexpr float kEpsilon = 1e-8f;
    const Vector3 edge1 = v1 - v0;
    const Vector3 edge2 = v2 - v0;
    const Vector3 pvec = dir.Cross(edge2);
    const float det = edge1.Dot(pvec);
    if (std::abs(det) < kEpsilon) return false;

    const float invDet = 1.0f / det;
    const Vector3 tvec = origin - v0;
    const float u = tvec.Dot(pvec) * invDet;
    if (u < 0.0f || u > 1.0f) return false;

    const Vector3 qvec = tvec.Cross(edge1);
    const float v = dir.Dot(qvec) * invDet;
    if (v < 0.0f || u + v > 1.0f) return false;

    const float t = edge2.Dot(qvec) * invDet;
    if (t < 0.0f || t > 1.0f) return false;

    outT = t;
    return true;
}

/// @brief TextRendererの各文字インスタンス（ワールド行列）から、テキスト全体の外接矩形
///        （ワールド空間AABB）を求める（UIButton::ComputeTextWorldBoundsと同じ考え方）
/// @details TextRendererはMeshFilterを持たない（1文字＝1インスタンスの動的描画）ため、
///          三角形ピッキング（RaycastSceneMeshes内のMeshFilter必須ループ）の対象にできず、
///          このAABBをスクリーン空間へ投影したバウンディングボックス判定でクリック選択する
bool ComputeTextRendererWorldBounds(const TextRenderer *textRenderer, Vector3 &outMin, Vector3 &outMax) {
    const auto instances = textRenderer->GetRenderInstances();
    if (instances.empty()) return false;

    const Vector3 corners[4] = {
        Vector3(-0.5f, -0.5f, 0.0f), Vector3(0.5f, -0.5f, 0.0f),
        Vector3(-0.5f, 0.5f, 0.0f), Vector3(0.5f, 0.5f, 0.0f),
    };

    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    for (const auto &instance : instances) {
        for (const auto &corner : corners) {
            const Vector3 p = corner.Transform(instance.worldMatrix);
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }
    }
    outMin = Vector3(minX, minY, 0.0f);
    outMax = Vector3(maxX, maxY, 0.0f);
    return true;
}
} // namespace

EmptyObject *SceneEditorView::PickIconAtScreenPosition(const ImVec2 &screenPos, const ImVec2 &imagePos, const ImVec2 &imageSize) const {
    if (!context_) return nullptr;
    auto *sceneRenderer = context_->GetComponent<SceneRenderer>();
    if (!sceneRenderer) return nullptr;

    // アイコンの見た目の大きさ（DrawLightMarkers/DrawCameraMarkersの描画半径）より少し広めに判定する
    constexpr float kIconPickRadius = 12.0f;
    EmptyObject *picked = nullptr;
    float nearestDistSq = kIconPickRadius * kIconPickRadius;

    auto considerIcon = [&](const Vector3 &worldPosition, const EmptyObject *ownerObject) {
        if (!ownerObject) return;
        ImVec2 iconScreenPos;
        if (!ProjectToImage(worldPosition, imagePos, imageSize, iconScreenPos)) return;
        const float dx = iconScreenPos.x - screenPos.x;
        const float dy = iconScreenPos.y - screenPos.y;
        const float distSq = dx * dx + dy * dy;
        if (distSq > nearestDistSq) return;
        nearestDistSq = distSq;
        picked = context_->GetSceneObject(ownerObject->GetObjectID());
    };

    // マーカー表示が無効な種別・表示モード上アイコンが出ていない種別は、クリック判定からも除外する
    // （ライトは3D専用のためDrawLightMarkersと同様に2Dモードでは対象外）
    if (showLightMarkers_ && displayMode_ != SceneRenderer::EditorDisplayMode::TwoDOnly) {
        for (auto *lightRenderer : sceneRenderer->GetLightRenderers()) {
            if (!lightRenderer || !lightRenderer->IsActive()) continue;
            considerIcon(lightRenderer->GetWorldPosition(), lightRenderer->GetOwnerObject());
        }
    }
    if (showCameraMarkers_) {
        // DrawCameraMarkersと同じ規則で、表示モードに応じてCamera2D/Camera3Dの対象を絞り込む
        const bool showThreeDCameras = displayMode_ != SceneRenderer::EditorDisplayMode::TwoDOnly;
        const bool showTwoDCameras = displayMode_ != SceneRenderer::EditorDisplayMode::ThreeDOnly;
        for (auto *cameraRenderer : sceneRenderer->GetCameraRenderers()) {
            if (!cameraRenderer || !cameraRenderer->IsActive()) continue;
            const auto *owner = cameraRenderer->GetOwnerObject();
            const bool isTwoD = owner && owner->GetComponent<Camera2D>();
            const bool isThreeD = owner && owner->GetComponent<Camera3D>();
            if (isTwoD && !showTwoDCameras) continue;
            if (isThreeD && !showThreeDCameras) continue;
            if (!isTwoD && !isThreeD) continue;
            considerIcon(cameraRenderer->GetWorldPosition(), cameraRenderer->GetOwnerObject());
        }
    }

    return picked;
}

bool SceneEditorView::RaycastSceneMeshes(const ImVec2 &screenPos, const ImVec2 &imagePos, const ImVec2 &imageSize,
    Vector3 &outRayStart, Vector3 &outRayEnd, EmptyObject *&outHitObject, float &outHitT) const {
    outHitObject = nullptr;
    outHitT = std::numeric_limits<float>::max();
    if (!context_ || imageSize.x <= 0.0f || imageSize.y <= 0.0f) return false;

    // クリック位置からカメラの近平面→遠平面を貫く線分を作る（tがそのまま奥行き順の比較に使える）
    const float ndcX = ((screenPos.x - imagePos.x) / imageSize.x) * 2.0f - 1.0f;
    const float ndcY = -(((screenPos.y - imagePos.y) / imageSize.y) * 2.0f - 1.0f);
    const Matrix4x4 invViewProjection = (GetActiveView() * GetActiveProjection()).Inverse();
    outRayStart = UnprojectNdc(invViewProjection, ndcX, ndcY, 0.0f);
    outRayEnd = UnprojectNdc(invViewProjection, ndcX, ndcY, 1.0f);

    // 「2D」モード限定のフォールバック用: 三角形との厳密な交差判定が1つもヒットしなかった場合に使う、
    // 画面空間での近さによる救済判定。ズームアウトするほどスプライトが画面上で小さくなり、
    // ピクセル単位で完全に正確なジオメトリ判定ではクリックがどんどんシビアになってしまう
    // （2D編集は3Dよりも「多数の小さいオブジェクトを広い範囲から選ぶ」場面が多いため影響が大きい）。
    // アイコンピッキング（PickIconAtScreenPosition）と同様の考え方で、スクリーン空間の
    // バウンディングボックスに数ピクセルの許容範囲を持たせて拾う
    constexpr float kSpriteClickTolerancePx = 6.0f;
    EmptyObject *fallbackObject = nullptr;
    float fallbackBestAreaPx = std::numeric_limits<float>::max();
    Vector3 fallbackWorldPosition{};

    for (auto *obj : context_->GetSceneObjects()) {
        // SceneRenderer側の描画除外（IsHiddenFromEditorTarget）と揃える。描画されていないのに
        // クリックだけは通ってしまう不整合を防ぐ
        if (!obj || !obj->IsActive() || obj->IsHiddenFromEditorTarget()) continue;

        // シーンビューに描画される対象（アクティブなMeshRenderer/SkinnedMeshRenderer/SpriteRendererを持つ）
        // だけを判定対象にする。SpriteRendererはエディターのシーンビュー上でのみ3D空間内に配置される
        // 特殊描画（SceneRenderer.cppのResolveEditorWorldPipelineName参照）と対になる、クリック選択対応
        auto *meshFilter = obj->GetComponent<MeshFilter>();
        if (!meshFilter || !meshFilter->HasMesh()) continue;
        auto *meshRenderer = obj->GetComponent<MeshRenderer>();
        auto *skinnedMeshRenderer = obj->GetComponent<SkinnedMeshRenderer>();
        auto *spriteRenderer = obj->GetComponent<SpriteRenderer>();
        // シーンビューの表示モード（ツールバー）に応じて選択対象を絞り込む
        const bool threeDVisible = displayMode_ != SceneRenderer::EditorDisplayMode::TwoDOnly;
        const bool twoDVisible = displayMode_ != SceneRenderer::EditorDisplayMode::ThreeDOnly;
        const bool hasVisibleRenderer =
            (threeDVisible && meshRenderer && meshRenderer->IsActive()) ||
            (threeDVisible && skinnedMeshRenderer && skinnedMeshRenderer->IsActive()) ||
            (twoDVisible && spriteRenderer && spriteRenderer->IsActive());
        if (!hasVisibleRenderer) continue;

        // SpriteRendererはアンカー/ピボット補正込みのワールド行列を使う必要があるため、
        // MeshRenderer/SkinnedMeshRenderer（Transformのワールド行列そのまま）とは取得元を分ける
        auto *transform = obj->GetComponent<Transform>();
        const Matrix4x4 world = (spriteRenderer && !meshRenderer && !skinnedMeshRenderer)
            ? spriteRenderer->GetWorldMatrix()
            : (transform ? transform->GetWorldMatrix() : Matrix4x4::Identity());

        // レイをオブジェクトのローカル空間へ変換して三角形と判定する
        // （アフィン変換では線分上のパラメータtが保存されるため、tはワールド空間の奥行き比較にそのまま使える）
        const Matrix4x4 invWorld = world.Inverse();
        const Vector3 localStart = TransformPoint(outRayStart, invWorld);
        const Vector3 localDir = TransformPoint(outRayEnd, invWorld) - localStart;

        const auto &model = ModelManager::GetModelData(meshFilter->GetMeshHandle());
        const auto &vertices = model.GetVertices();
        const auto &indices = model.GetIndices();

        // 2Dモードのフォールバック候補として、このオブジェクトの画面空間バウンディングボックスを求める
        // （SpriteRendererのみ対象。3Dオブジェクトは既存の厳密な三角形判定のみで十分なため対象外）
        if (displayMode_ == SceneRenderer::EditorDisplayMode::TwoDOnly &&
            spriteRenderer && !meshRenderer && !skinnedMeshRenderer) {
            float minX = std::numeric_limits<float>::max();
            float minY = std::numeric_limits<float>::max();
            float maxX = -std::numeric_limits<float>::max();
            float maxY = -std::numeric_limits<float>::max();
            bool anyProjected = false;
            for (const auto &v : vertices) {
                ImVec2 screenVertex;
                if (!ProjectToImage(TransformPoint(Vector3(v.px, v.py, v.pz), world), imagePos, imageSize, screenVertex, false)) continue;
                minX = std::min(minX, screenVertex.x);
                minY = std::min(minY, screenVertex.y);
                maxX = std::max(maxX, screenVertex.x);
                maxY = std::max(maxY, screenVertex.y);
                anyProjected = true;
            }
            if (anyProjected) {
                minX -= kSpriteClickTolerancePx; minY -= kSpriteClickTolerancePx;
                maxX += kSpriteClickTolerancePx; maxY += kSpriteClickTolerancePx;
                if (screenPos.x >= minX && screenPos.x <= maxX && screenPos.y >= minY && screenPos.y <= maxY) {
                    const float areaPx = (maxX - minX) * (maxY - minY);
                    // 複数候補が範囲内にある場合、画面上でより小さい（＝より的を絞ってクリックしたであろう）
                    // ものを優先する
                    if (areaPx < fallbackBestAreaPx) {
                        fallbackBestAreaPx = areaPx;
                        fallbackObject = obj;
                        fallbackWorldPosition = Vector3(world.m[3][0], world.m[3][1], world.m[3][2]);
                    }
                }
            }
        }

        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            const size_t indexA = static_cast<size_t>(indices[i]);
            const size_t indexB = static_cast<size_t>(indices[i + 1]);
            const size_t indexC = static_cast<size_t>(indices[i + 2]);
            // インポート失敗や編集中のアセット差し替えで壊れたインデックスが混ざっても、
            // シーンビューのレイ判定から頂点配列を範囲外参照しない。
            if (indexA >= vertices.size() || indexB >= vertices.size() || indexC >= vertices.size()) {
                continue;
            }
            const auto &a = vertices[indexA];
            const auto &b = vertices[indexB];
            const auto &c = vertices[indexC];
            float t = 0.0f;
            if (SegmentIntersectsTriangle(localStart, localDir,
                    Vector3(a.px, a.py, a.pz), Vector3(b.px, b.py, b.pz), Vector3(c.px, c.py, c.pz), t)) {
                if (t < outHitT) {
                    outHitT = t;
                    outHitObject = obj;
                }
            }
        }
    }

    // TextRendererはMeshFilterを持たない（1文字＝1インスタンスの動的描画）ため、上のMeshFilter必須
    // ループでは判定できない。SpriteRendererと違い2D/3Dどちらの配置でも使われるコンポーネントのため、
    // 表示モードに関わらずスクリーン空間バウンディングボックスのフォールバック判定のみで拾う
    for (auto *obj : context_->GetSceneObjects()) {
        // SceneRenderer側の描画除外（IsHiddenFromEditorTarget）と揃える。描画されていないのに
        // クリックだけは通ってしまう不整合を防ぐ
        if (!obj || !obj->IsActive() || obj->IsHiddenFromEditorTarget()) continue;
        auto *textRenderer = obj->GetComponent<TextRenderer>();
        if (!textRenderer || !textRenderer->IsActive()) continue;

        Vector3 textBoundsMin{};
        Vector3 textBoundsMax{};
        if (!ComputeTextRendererWorldBounds(textRenderer, textBoundsMin, textBoundsMax)) continue;

        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float maxX = -std::numeric_limits<float>::max();
        float maxY = -std::numeric_limits<float>::max();
        bool anyProjected = false;
        const Vector3 worldCorners[4] = {
            Vector3(textBoundsMin.x, textBoundsMin.y, 0.0f), Vector3(textBoundsMax.x, textBoundsMin.y, 0.0f),
            Vector3(textBoundsMin.x, textBoundsMax.y, 0.0f), Vector3(textBoundsMax.x, textBoundsMax.y, 0.0f),
        };
        for (const auto &corner : worldCorners) {
            ImVec2 screenVertex;
            if (!ProjectToImage(corner, imagePos, imageSize, screenVertex, false)) continue;
            minX = std::min(minX, screenVertex.x);
            minY = std::min(minY, screenVertex.y);
            maxX = std::max(maxX, screenVertex.x);
            maxY = std::max(maxY, screenVertex.y);
            anyProjected = true;
        }
        if (!anyProjected) continue;

        minX -= kSpriteClickTolerancePx; minY -= kSpriteClickTolerancePx;
        maxX += kSpriteClickTolerancePx; maxY += kSpriteClickTolerancePx;
        if (screenPos.x < minX || screenPos.x > maxX || screenPos.y < minY || screenPos.y > maxY) continue;

        const float areaPx = (maxX - minX) * (maxY - minY);
        if (areaPx < fallbackBestAreaPx) {
            fallbackBestAreaPx = areaPx;
            fallbackObject = obj;
            fallbackWorldPosition = (textBoundsMin + textBoundsMax) * 0.5f;
        }
    }

    // 三角形との厳密な交差判定がどれもヒットしなかった場合のみ、バウンディングボックスによる
    // フォールバック候補（2DモードのSpriteRenderer、表示モードに関わらないTextRenderer）を採用する
    if (!outHitObject && fallbackObject) {
        outHitObject = fallbackObject;
        // レイ上でオブジェクトの位置に最も近い点のtを、外接する三角形が無くても求まる方法
        // （レイへの正射影）で概算する。ComputeCursorWorldPosition等がこのtを深度として使うため、
        // 0での初期化のまま（＝レイの始点扱い）にはしない
        const Vector3 rayDir = outRayEnd - outRayStart;
        const float rayDirLengthSq = rayDir.LengthSquared();
        if (rayDirLengthSq > 1e-8f) {
            const float t = (fallbackWorldPosition - outRayStart).Dot(rayDir) / rayDirLengthSq;
            outHitT = std::clamp(t, 0.0f, 1.0f);
        } else {
            outHitT = 0.0f;
        }
    }

    return outHitObject != nullptr;
}

Vector3 SceneEditorView::ComputeCursorWorldPosition(const ImVec2 &screenPos, const ImVec2 &imagePos, const ImVec2 &imageSize) const {
    Vector3 rayStart{};
    Vector3 rayEnd{};
    EmptyObject *hitObject = nullptr;
    float hitT = 0.0f;
    if (RaycastSceneMeshes(screenPos, imagePos, imageSize, rayStart, rayEnd, hitObject, hitT)) {
        return rayStart + (rayEnd - rayStart) * hitT;
    }

    // メッシュに当たらなかった場合はY=0の地面平面との交点へ配置する（Unityのシーンビューと同様の挙動）
    const Vector3 rayDir = rayEnd - rayStart;
    if (std::abs(rayDir.y) > 1e-6f) {
        const float s = -rayStart.y / rayDir.y;
        if (s > 0.0f) {
            return rayStart + rayDir * s;
        }
    }

    // 地面平面とも交差しない場合（真上/真下を向いている等）は、カメラから現在の注視距離だけ進めた点にする
    const Vector3 rayDirNormalized = rayDir.Length() > 1e-6f ? rayDir.Normalize() : Vector3(0.0f, 0.0f, 1.0f);
    return cameraEye_ + rayDirNormalized * distance_;
}

bool SceneEditorView::ComputeTilemapCellUnderCursor(EmptyObject *owner, TilemapRenderer *tilemap, const ImVec2 &screenPos,
    const ImVec2 &imagePos, const ImVec2 &imageSize, int &outX, int &outY) const {
    if (!owner || !tilemap || imageSize.x <= 0.0f || imageSize.y <= 0.0f) return false;
    auto *transform = owner->GetComponent<Transform>();
    if (!transform) return false;

    // クリック位置からカメラの近平面→遠平面を貫くワールド空間のレイを作る（RaycastSceneMeshesと同じ式）
    const float ndcX = ((screenPos.x - imagePos.x) / imageSize.x) * 2.0f - 1.0f;
    const float ndcY = -(((screenPos.y - imagePos.y) / imageSize.y) * 2.0f - 1.0f);
    const Matrix4x4 invViewProjection = (GetActiveView() * GetActiveProjection()).Inverse();
    const Vector3 rayStart = UnprojectNdc(invViewProjection, ndcX, ndcY, 0.0f);
    const Vector3 rayEnd = UnprojectNdc(invViewProjection, ndcX, ndcY, 1.0f);

    // レイをTilemapRendererのローカル空間（メッシュが生成されるローカルZ=0平面）へ変換して交点を求める
    const Matrix4x4 invWorld = transform->GetWorldMatrix().Inverse();
    const Vector3 localStart = TransformPoint(rayStart, invWorld);
    const Vector3 localEnd = TransformPoint(rayEnd, invWorld);
    const Vector3 localDir = localEnd - localStart;
    if (std::abs(localDir.z) < 1e-8f) return false; // レイがZ=0平面とほぼ平行
    const float t = -localStart.z / localDir.z;
    if (t < 0.0f || t > 1.0f) return false; // 近平面〜遠平面の間で交差しない

    const float localX = localStart.x + localDir.x * t;
    const float localY = localStart.y + localDir.y * t;

    const Vector2 &tileSize = tilemap->GetTileSize();
    if (tileSize.x <= 0.0f || tileSize.y <= 0.0f) return false;
    const int cellX = static_cast<int>(std::floor(localX / tileSize.x));
    const int cellY = static_cast<int>(std::floor(localY / tileSize.y));
    if (cellX < 0 || cellY < 0 || cellX >= tilemap->GetGridWidth() || cellY >= tilemap->GetGridHeight()) return false;

    outX = cellX;
    outY = cellY;
    return true;
}

void SceneEditorView::DrawTilemapCellHighlight(EmptyObject *owner, TilemapRenderer *tilemap, int cellX, int cellY,
    const ImVec2 &imagePos, const ImVec2 &imageSize, ImU32 color) const {
    auto *transform = owner ? owner->GetComponent<Transform>() : nullptr;
    if (!transform) return;
    const Matrix4x4 &world = transform->GetWorldMatrix();
    const Vector2 &tileSize = tilemap->GetTileSize();

    const float x0 = static_cast<float>(cellX) * tileSize.x;
    const float y0 = static_cast<float>(cellY) * tileSize.y;
    const float x1 = x0 + tileSize.x;
    const float y1 = y0 + tileSize.y;
    const Vector3 localCorners[4] = {
        Vector3(x0, y0, 0.0f), Vector3(x0, y1, 0.0f), Vector3(x1, y1, 0.0f), Vector3(x1, y0, 0.0f),
    };

    ImVec2 screenCorners[4];
    for (int i = 0; i < 4; ++i) {
        if (!ProjectToImage(TransformPoint(localCorners[i], world), imagePos, imageSize, screenCorners[i], false)) return;
    }

    auto *drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(imagePos, ImVec2(imagePos.x + imageSize.x, imagePos.y + imageSize.y), true);
    drawList->AddQuad(screenCorners[0], screenCorners[1], screenCorners[2], screenCorners[3], color, 2.0f);
    drawList->PopClipRect();
}

bool SceneEditorView::HandleTilemapPaint(EmptyObject *owner, TilemapRenderer *tilemap, SceneEditorCommands *commands,
    const ImVec2 &imagePos, const ImVec2 &imageSize) {
    if (!owner || !tilemap) return false;
    const bool isHovered = ImGui::IsItemHovered();

    int cellX = 0;
    int cellY = 0;
    const bool hasHoverCell = isHovered && ComputeTilemapCellUnderCursor(owner, tilemap, ImGui::GetMousePos(), imagePos, imageSize, cellX, cellY);

    if (hasHoverCell) {
        constexpr ImU32 kHighlightColor = IM_COL32(255, 220, 60, 255);
        DrawTilemapCellHighlight(owner, tilemap, cellX, cellY, imagePos, imageSize, kHighlightColor);
    }

    // 右クリック消しゴムは「2D」表示モード限定にする。Combined/ThreeDOnlyモードでは右ドラッグが
    // 既に3Dフリーカメラの見回し操作（HandleCameraInput参照）に使われており、同時に奪うと
    // カメラ操作ができなくなってしまうため（2Dモードは右ボタンをカメラ操作に使っていないため競合しない）
    const bool rightEraseAvailable = displayMode_ == SceneRenderer::EditorDisplayMode::TwoDOnly;
    const bool leftClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    const bool rightClicked = rightEraseAvailable && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    const bool leftDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    const bool rightDown = rightEraseAvailable && ImGui::IsMouseDown(ImGuiMouseButton_Right);
    const bool leftReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
    const bool rightReleased = rightEraseAvailable && ImGui::IsMouseReleased(ImGuiMouseButton_Right);

    if (hasHoverCell && (leftClicked || rightClicked)) {
        // ストローク開始: 右クリックで始めた場合は選択中のブラシに関わらず常に消しゴム（-1）として塗る。
        // Undo用に変更前のJSONを1回だけ取得する（ギズモ操作開始時と同じパターン）
        isPaintStrokeActive_ = true;
        paintStrokeIsErase_ = rightClicked;
        paintStrokeBeforeJson_ = owner->SaveComponentToJson(tilemap);
        tilemap->SetTile(cellX, cellY, paintStrokeIsErase_ ? -1 : paintBrushTileType_);
        lastPaintCellX_ = cellX;
        lastPaintCellY_ = cellY;
    } else if (isPaintStrokeActive_ && hasHoverCell && (leftDown || rightDown)) {
        // ドラッグ中: 前フレームのセルから今フレームのセルまでを線で補間して塗る
        const int brush = paintStrokeIsErase_ ? -1 : paintBrushTileType_;
        WalkGridLine(lastPaintCellX_, lastPaintCellY_, cellX, cellY, [&](int x, int y) {
            tilemap->SetTile(x, y, brush);
        });
        lastPaintCellX_ = cellX;
        lastPaintCellY_ = cellY;
    }

    if (isPaintStrokeActive_ && (leftReleased || rightReleased)) {
        // ストローク終了: 変更があった場合のみUndo履歴へ積む（ギズモ操作終了時と同じ「適用済みの
        // 操作を積む」パターン。PushExecutedはSceneEditorCommands.h参照）
        isPaintStrokeActive_ = false;
        if (commands) {
            JSON after = owner->SaveComponentToJson(tilemap);
            if (after != paintStrokeBeforeJson_) {
                commands->PushExecuted(std::make_unique<ComponentEditCommand>(owner, tilemap, paintStrokeBeforeJson_, after));
            }
        }
    }

    return hasHoverCell || isPaintStrokeActive_;
}

void SceneEditorView::ShowTilemapPaintToolbar(TilemapRenderer *paintableTilemap) {
    ImGui::BeginDisabled(paintableTilemap == nullptr);
    if (ImGui::Checkbox(TranslationLabel("editor.sceneview.tilepaint.enable"), &tilemapPaintActive_)) {
        // トグル切り替えの瞬間にストローク状態が残らないようにする
        isPaintStrokeActive_ = false;
    }
    ImGui::EndDisabled();

    if (!paintableTilemap) {
        ImGui::TextUnformatted(TranslationC("editor.sceneview.tilepaint.no_selection"));
        return;
    }

    // タイル種類が削除される等でブラシのインデックスが範囲外になった場合は安全な値へ戻す
    if (paintBrushTileType_ >= paintableTilemap->GetTileTypeCount()) {
        paintBrushTileType_ = (paintableTilemap->GetTileTypeCount() > 0) ? 0 : -1;
    }

    ImGui::SameLine();
    ImGui::TextUnformatted(TranslationC("editor.sceneview.tilepaint.brush"));
    ImGui::SameLine();
    if (ImGui::RadioButton(TranslationLabel("editor.sceneview.tilepaint.erase"), paintBrushTileType_ == -1)) {
        paintBrushTileType_ = -1;
    }

    // タイルセットのSRV・サイズが解決できればサムネイル画像で、できなければ番号ボタンでフォールバック表示する
    D3D12_GPU_DESCRIPTOR_HANDLE thumbnailSrv{};
    float texWidth = 0.0f;
    float texHeight = 0.0f;
    if (const auto *material = MaterialManager::GetMaterial(paintableTilemap->GetMaterialHandle())) {
        const auto textureView = TextureManager::GetTextureView(material->textureHandle);
        texWidth = static_cast<float>(textureView.GetWidth());
        texHeight = static_cast<float>(textureView.GetHeight());
        thumbnailSrv = textureView.GetSrvHandle();
    }
    const bool hasThumbnail = thumbnailSrv.ptr != 0 && texWidth > 0.0f && texHeight > 0.0f;
    const ImVec2 kThumbnailSize(32.0f, 32.0f);
    constexpr ImU32 kSelectedBorderColor = IM_COL32(255, 220, 60, 255);

    const auto &tileTypes = paintableTilemap->GetTileTypes();
    const Vector2 &tilePixelSize = paintableTilemap->GetTilePixelSize();
    for (int i = 0; i < static_cast<int>(tileTypes.size()); ++i) {
        ImGui::SameLine();
        ImGui::PushID(i);
        const bool isSelected = (paintBrushTileType_ == i);
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Border, kSelectedBorderColor);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
        }

        bool clicked = false;
        if (hasThumbnail) {
            // ビットマスク0（＝tilesetOriginPxそのもの。どのタイル種類にも必ず存在する孤立パターン）を
            // そのタイル種類の代表画像として表示する
            const Vector2 &origin = tileTypes[i].tilesetOriginPx;
            const ImVec2 uv0(origin.x / texWidth, origin.y / texHeight);
            const ImVec2 uv1((origin.x + tilePixelSize.x) / texWidth, (origin.y + tilePixelSize.y) / texHeight);
            clicked = ImGui::ImageButton("##tileThumb", static_cast<ImTextureID>(thumbnailSrv.ptr), kThumbnailSize, uv0, uv1);
        } else {
            clicked = ImGui::Button(std::to_string(i).c_str(), kThumbnailSize);
        }
        if (clicked) paintBrushTileType_ = i;

        if (isSelected) {
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }
        ImGui::PopID();
    }
}

void SceneEditorView::HandleObjectPicking(SceneObjectHierarchy *hierarchy, const ImVec2 &imagePos, const ImVec2 &imageSize) {
    if (!hierarchy || !context_ || imageSize.x <= 0.0f || imageSize.y <= 0.0f) return;
    // シーンビュー画像の上にマウスがある状態での、ドラッグを伴わない左クリック（離した瞬間）で選択する
    if (!ImGui::IsItemHovered()) return;
    if (!ImGui::IsMouseReleased(ImGuiMouseButton_Left)) return;
    // カメラ操作等のドラッグ後のリリースでは選択しない（Unityと同様、微小な移動はクリック扱い）
    constexpr float kClickMoveThreshold = 3.0f;
    if (ImGui::GetIO().MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] > kClickMoveThreshold * kClickMoveThreshold) return;
    // ギズモを操作している/ギズモの上をクリックした場合は選択を変えない。
    // ImGuizmo::IsOver()/IsUsing()は直前にManipulate()が呼ばれた際のスクリーン座標キャッシュを
    // 参照して判定するため、選択が無い（ShowGizmoがtargets.empty()で早期リターンし、Manipulate()が
    // 二度と呼ばれない）状態では、選択解除前にギズモが表示されていた古い画面位置がキャッシュされたまま
    // 残り続ける。その状態でマウスがたまたま古い位置に近いと、実際にはギズモが存在しないにもかかわらず
    // IsOver()がtrueを返し続け、クリックによる再選択が握りつぶされてしまう
    // （カメラを動かすと古いキャッシュ位置と実際のクリック位置がずれるため、症状が一時的に消えていた）。
    // gizmoTargetObjects_（ShowGizmoが選択集合が変わった時点で更新する）が空の場合は、
    // ImGuizmoの戻り値を無条件に信用せず無視する
    if (!gizmoTargetObjects_.empty() && (ImGuizmo::IsUsing() || ImGuizmo::IsOver())) return;

    const ImVec2 mouse = ImGui::GetMousePos();

    // メッシュを持たないLight/Camera等は、深度テストせず常に手前に描画されるアイコンとの
    // スクリーン座標距離でクリック判定する（メッシュの三角形ピッキングより優先する）。
    // 表示モードに応じた絞り込みはPickIconAtScreenPosition内部で行う
    EmptyObject *picked = PickIconAtScreenPosition(mouse, imagePos, imageSize);

    if (!picked) {
        Vector3 rayStart{};
        Vector3 rayEnd{};
        EmptyObject *hitObject = nullptr;
        float hitT = 0.0f;
        RaycastSceneMeshes(mouse, imagePos, imageSize, rayStart, rayEnd, hitObject, hitT);
        picked = hitObject;
    }

    // Ctrlクリックはトグル追加、通常クリックは単一選択（何もない場所なら選択解除）
    const bool additive = ImGui::IsKeyDown(ImGuiMod_Ctrl);
    if (picked) {
        hierarchy->SelectObject(picked, additive);
    } else if (!additive) {
        hierarchy->SelectObject(nullptr, false);
    }
}

void SceneEditorView::DrawCameraMarkers(const ImVec2 &imagePos, const ImVec2 &imageSize) {
    if (!context_ || imageSize.x <= 0.0f || imageSize.y <= 0.0f) return;
    auto *sceneRenderer = context_->GetComponent<SceneRenderer>();
    if (!sceneRenderer) return;

    auto *drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(imagePos, ImVec2(imagePos.x + imageSize.x, imagePos.y + imageSize.y), true);

    // 表示モードに応じて対象カメラの種類を絞り込む（2D/3D併用モードは両方、3D/2Dのみモードはそれぞれ対応する
    // 種類のみ）。Camera2D/Camera3Dどちらのコンポーネントも持たないCameraRenderer単体は対象外とする
    const bool showThreeDCameras = displayMode_ != SceneRenderer::EditorDisplayMode::TwoDOnly;
    const bool showTwoDCameras = displayMode_ != SceneRenderer::EditorDisplayMode::ThreeDOnly;

    for (auto *cameraRenderer : sceneRenderer->GetCameraRenderers()) {
        if (!cameraRenderer || !cameraRenderer->IsActive()) continue;

        const auto *owner = cameraRenderer->GetOwnerObject();
        const bool isTwoD = owner && owner->GetComponent<Camera2D>();
        const bool isThreeD = owner && owner->GetComponent<Camera3D>();
        if (isTwoD && !showTwoDCameras) continue;
        if (isThreeD && !showThreeDCameras) continue;
        if (!isTwoD && !isThreeD) continue;

        const Vector3 position = cameraRenderer->GetWorldPosition();
        ImVec2 center;
        if (!ProjectToImage(position, imagePos, imageSize, center)) continue;

        // 2Dカメラと3Dカメラでアイコンの色を分け、シーンビュー上でも見分けられるようにする
        const ImU32 color = isTwoD ? IM_COL32(255, 200, 110, 255) : IM_COL32(120, 200, 255, 255);

        // カメラ風アイコン（本体の四角＋レンズの円）
        constexpr float kHalfWidth = 8.0f;
        constexpr float kHalfHeight = 6.0f;
        drawList->AddRect(
            ImVec2(center.x - kHalfWidth, center.y - kHalfHeight),
            ImVec2(center.x + kHalfWidth, center.y + kHalfHeight),
            color, 2.0f, 0, 2.0f);
        drawList->AddCircle(center, 3.5f, color, 12, 1.5f);
    }

    drawList->PopClipRect();
}

void SceneEditorView::DrawGrid2D(const ImVec2 &imagePos, const ImVec2 &imageSize) {
    if (imageSize.x <= 0.0f || imageSize.y <= 0.0f || zoom2D_ <= 0.0f) return;

    auto *drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(imagePos, ImVec2(imagePos.x + imageSize.x, imagePos.y + imageSize.y), true);

    // 画面上でおよそこの間隔(px)になるよう、1-2-5系列(1,2,5,10,20,50...)から間隔(ワールド単位)を選ぶ
    // （Photoshop/Figma等の2Dエディターでよく使われる、キリの良い数値になる方式）
    constexpr float kTargetPixelSpacing = 64.0f;
    const float worldPerPixel = (zoom2D_ * 2.0f) / imageSize.y;
    const float rawSpacing = std::max(kTargetPixelSpacing * worldPerPixel, 1e-6f);
    const float exponent = std::floor(std::log10(rawSpacing));
    const float base = std::pow(10.0f, exponent);
    float spacing = base * 10.0f;
    for (const float mult : { 1.0f, 2.0f, 5.0f, 10.0f }) {
        if (base * mult >= rawSpacing) {
            spacing = base * mult;
            break;
        }
    }

    const float aspect = imageSize.x / imageSize.y;
    const float halfHeight = zoom2D_;
    const float halfWidth = zoom2D_ * aspect;
    const float minX = pan2D_.x - halfWidth, maxX = pan2D_.x + halfWidth;
    const float minY = pan2D_.y - halfHeight, maxY = pan2D_.y + halfHeight;

    constexpr ImU32 kLineColor = IM_COL32(255, 255, 255, 30);
    constexpr ImU32 kAxisColorX = IM_COL32(220, 80, 80, 160); // Y=0の軸線
    constexpr ImU32 kAxisColorY = IM_COL32(80, 200, 80, 160); // X=0の軸線
    constexpr float kAxisEpsilon = 1e-4f;

    const int startI = static_cast<int>(std::floor(minX / spacing));
    const int endI = static_cast<int>(std::ceil(maxX / spacing));
    for (int i = startI; i <= endI; ++i) {
        const float x = static_cast<float>(i) * spacing;
        ImVec2 p0, p1;
        if (ProjectToImage(Vector3(x, minY, 0.0f), imagePos, imageSize, p0, false) &&
            ProjectToImage(Vector3(x, maxY, 0.0f), imagePos, imageSize, p1, false)) {
            const bool isAxis = std::abs(x) < kAxisEpsilon;
            drawList->AddLine(p0, p1, isAxis ? kAxisColorY : kLineColor, isAxis ? 1.5f : 1.0f);
        }
    }
    const int startJ = static_cast<int>(std::floor(minY / spacing));
    const int endJ = static_cast<int>(std::ceil(maxY / spacing));
    for (int j = startJ; j <= endJ; ++j) {
        const float y = static_cast<float>(j) * spacing;
        ImVec2 p0, p1;
        if (ProjectToImage(Vector3(minX, y, 0.0f), imagePos, imageSize, p0, false) &&
            ProjectToImage(Vector3(maxX, y, 0.0f), imagePos, imageSize, p1, false)) {
            const bool isAxis = std::abs(y) < kAxisEpsilon;
            drawList->AddLine(p0, p1, isAxis ? kAxisColorX : kLineColor, isAxis ? 1.5f : 1.0f);
        }
    }

    drawList->PopClipRect();
}

void SceneEditorView::AppendCameraFrustumLines(std::vector<DebugLineVertex> &out) {
    if (!context_) return;
    auto *sceneRenderer = context_->GetComponent<SceneRenderer>();
    if (!sceneRenderer) return;

    constexpr Vector4 kNearColorThreeD{ 0.47f, 0.78f, 1.0f, 1.0f };
    constexpr Vector4 kFarColorThreeD{ 0.47f, 0.78f, 1.0f, 0.6f };
    // Camera2Dはオレンジ系にして、DrawCameraMarkersのアイコン色分けと揃える
    constexpr Vector4 kNearColorTwoD{ 1.0f, 0.78f, 0.43f, 1.0f };
    constexpr Vector4 kFarColorTwoD{ 1.0f, 0.78f, 0.43f, 0.6f };

    for (auto *cameraRenderer : sceneRenderer->GetCameraRenderers()) {
        if (!cameraRenderer || !cameraRenderer->IsActive()) continue;
        const auto *owner = cameraRenderer->GetOwnerObject();
        const bool isTwoD = owner && owner->GetComponent<Camera2D>();
        const bool isThreeD = owner && owner->GetComponent<Camera3D>();
        if (!isTwoD && !isThreeD) continue;
        const Vector4 &kNearColor = isTwoD ? kNearColorTwoD : kNearColorThreeD;
        const Vector4 &kFarColor = isTwoD ? kFarColorTwoD : kFarColorThreeD;

        const Matrix4x4 invViewProjection = cameraRenderer->GetViewProjectionMatrix().Inverse();

        // NDCの4隅（近平面 z=0 / 遠平面 z=1、D3Dの深度レンジ）をワールド座標へ逆投影する
        static constexpr float kNdcXY[4][2] = { {-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f} };
        Vector3 nearCorners[4];
        Vector3 farCorners[4];
        for (int i = 0; i < 4; ++i) {
            nearCorners[i] = UnprojectNdc(invViewProjection, kNdcXY[i][0], kNdcXY[i][1], 0.0f);
            farCorners[i] = UnprojectNdc(invViewProjection, kNdcXY[i][0], kNdcXY[i][1], 1.0f);
        }

        for (int i = 0; i < 4; ++i) {
            const int next = (i + 1) % 4;
            out.push_back({ nearCorners[i], kNearColor });
            out.push_back({ nearCorners[next], kNearColor });
            out.push_back({ farCorners[i], kFarColor });
            out.push_back({ farCorners[next], kFarColor });
            out.push_back({ nearCorners[i], kNearColor });
            out.push_back({ farCorners[i], kFarColor });
        }
    }
}

void SceneEditorView::AppendLightDirectionLines(std::vector<DebugLineVertex> &out) {
    if (!context_) return;
    auto *sceneRenderer = context_->GetComponent<SceneRenderer>();
    if (!sceneRenderer) return;

    for (auto *lightRenderer : sceneRenderer->GetLightRenderers()) {
        if (!lightRenderer || !lightRenderer->IsActive()) continue;
        auto *light = lightRenderer->GetLight();
        const auto lightType = light ? light->GetType() : Light::Type::Directional;
        // 方向を持つライトのみ向きの線を描画する
        if (lightType != Light::Type::Directional && lightType != Light::Type::Spot) continue;

        Vector4 color{ 1.0f, 0.86f, 0.38f, 1.0f };
        if (light) {
            const Vector4 &lightColor = light->GetColor();
            color = Vector4(
                std::clamp(lightColor.x, 0.0f, 1.0f),
                std::clamp(lightColor.y, 0.0f, 1.0f),
                std::clamp(lightColor.z, 0.0f, 1.0f),
                1.0f);
        }

        const Vector3 position = lightRenderer->GetWorldPosition();
        const Vector3 direction = lightRenderer->GetWorldDirection();
        const float length = (lightType == Light::Type::Spot && light) ? light->GetDistance() : 2.0f;
        const Vector3 tip = position + direction * length;
        out.push_back({ position, color });
        out.push_back({ tip, color });
        // 先端に小さな球を描いて向きの終端を示す
        AppendWireSphere3D(out, tip, 0.06f, color);
    }
}

void SceneEditorView::AppendSkeletonBoneLines(std::vector<DebugLineVertex> &out) {
    if (!context_) return;
    auto *sceneRenderer = context_->GetComponent<SceneRenderer>();
    if (!sceneRenderer) return;

    constexpr Vector4 kBoneColor{ 0.55f, 0.92f, 0.55f, 1.0f };
    constexpr Vector4 kJointColor{ 0.35f, 0.75f, 1.0f, 1.0f };

    for (auto *skinnedMeshRenderer : sceneRenderer->GetSkinnedMeshRenderers()) {
        if (!skinnedMeshRenderer || !skinnedMeshRenderer->IsActive()) continue;
        const auto joints = skinnedMeshRenderer->GetDebugJointInfos();
        if (joints.empty()) continue;

        // ジョイント球の半径はスケルトン全体の大きさに応じて自動調整する
        Vector3 minPos = joints.front().position;
        Vector3 maxPos = joints.front().position;
        for (const auto &joint : joints) {
            minPos.x = std::min(minPos.x, joint.position.x);
            minPos.y = std::min(minPos.y, joint.position.y);
            minPos.z = std::min(minPos.z, joint.position.z);
            maxPos.x = std::max(maxPos.x, joint.position.x);
            maxPos.y = std::max(maxPos.y, joint.position.y);
            maxPos.z = std::max(maxPos.z, joint.position.z);
        }
        const float diagonal = (maxPos - minPos).Length();
        const float jointRadius = std::clamp(diagonal * 0.01f, 0.005f, 0.1f);

        for (const auto &joint : joints) {
            AppendWireSphere3D(out, joint.position, jointRadius, kJointColor);
            if (joint.parentIndex >= 0 && static_cast<size_t>(joint.parentIndex) < joints.size()) {
                out.push_back({ joints[static_cast<size_t>(joint.parentIndex)].position, kBoneColor });
                out.push_back({ joint.position, kBoneColor });
            }
        }
    }
}

void SceneEditorView::DrawCamera2DBoundsOverlay(const ImVec2 &imagePos, const ImVec2 &imageSize) {
    if (!context_ || imageSize.x <= 0.0f || imageSize.y <= 0.0f) return;
    auto *sceneRenderer = context_->GetComponent<SceneRenderer>();
    if (!sceneRenderer) return;

    // AppendCameraFrustumLinesのCamera2D用配色と揃える
    constexpr ImU32 kColor = IM_COL32(255, 200, 110, 220);

    auto *drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(imagePos, ImVec2(imagePos.x + imageSize.x, imagePos.y + imageSize.y), true);

    for (auto *cameraRenderer : sceneRenderer->GetCameraRenderers()) {
        if (!cameraRenderer || !cameraRenderer->IsActive()) continue;
        const auto *owner = cameraRenderer->GetOwnerObject();
        if (!owner || !owner->GetComponent<Camera2D>()) continue;

        // near平面のNDC4隅をワールド座標へ逆投影する（Camera2Dは正射影のため、near/farどちらの
        // 平面でもXY範囲は同一。2Dモードでは奥行きを見せる意味が薄いのでnear面の矩形のみ表示する）
        const Matrix4x4 invViewProjection = cameraRenderer->GetViewProjectionMatrix().Inverse();
        static constexpr float kNdcXY[4][2] = { {-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f} };
        ImVec2 screenCorners[4];
        bool allValid = true;
        for (int i = 0; i < 4 && allValid; ++i) {
            const Vector3 corner = UnprojectNdc(invViewProjection, kNdcXY[i][0], kNdcXY[i][1], 0.0f);
            allValid = ProjectToImage(corner, imagePos, imageSize, screenCorners[i], false);
        }
        if (!allValid) continue;

        for (int i = 0; i < 4; ++i) {
            const int next = (i + 1) % 4;
            drawList->AddLine(screenCorners[i], screenCorners[next], kColor, 2.0f);
        }
    }

    drawList->PopClipRect();
}

void SceneEditorView::DrawCollider2DOverlay(const ImVec2 &imagePos, const ImVec2 &imageSize) {
    if (!context_ || imageSize.x <= 0.0f || imageSize.y <= 0.0f) return;
    auto *sceneObjectCollider = context_->GetComponent<SceneObjectCollider>();
    if (!sceneObjectCollider) return;

    // AppendColliderDebugLinesと同じ配色（GPUデバッグライン版と2Dオーバーレイ版で見た目を揃える）
    constexpr Vector4 kSolidColor{ 0.31f, 0.90f, 0.47f, 1.0f };
    constexpr Vector4 kTriggerColor{ 1.0f, 0.78f, 0.16f, 1.0f };

    // 形状の頂点列自体はAppendCollider2DShape（ワールド空間の線分リスト）をそのまま流用し、
    // ここでは2D専用カメラへの投影とImGui描画だけを行う（3D側と形状ロジックを二重管理しない）
    std::vector<DebugLineVertex> lines;
    for (auto *collider : sceneObjectCollider->GetRegisteredColliders()) {
        if (!collider || !collider->IsActive() || !collider->Is2D()) continue;
        const Vector4 &color = collider->IsTrigger() ? kTriggerColor : kSolidColor;
        if (auto info = collider->BuildColliderInfo2D()) {
            AppendCollider2DShape(lines, *info, collider->GetOwnerWorldPosition().z, color);
        }
    }
    if (lines.empty()) return;

    auto *drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(imagePos, ImVec2(imagePos.x + imageSize.x, imagePos.y + imageSize.y), true);
    for (size_t i = 0; i + 1 < lines.size(); i += 2) {
        ImVec2 p0, p1;
        if (!ProjectToImage(lines[i].position, imagePos, imageSize, p0, false)) continue;
        if (!ProjectToImage(lines[i + 1].position, imagePos, imageSize, p1, false)) continue;
        const Vector4 &c = lines[i].color;
        const ImU32 col = IM_COL32(
            static_cast<int>(std::clamp(c.x, 0.0f, 1.0f) * 255.0f),
            static_cast<int>(std::clamp(c.y, 0.0f, 1.0f) * 255.0f),
            static_cast<int>(std::clamp(c.z, 0.0f, 1.0f) * 255.0f),
            static_cast<int>(std::clamp(c.w, 0.0f, 1.0f) * 255.0f));
        drawList->AddLine(p0, p1, col, 2.0f);
    }
    drawList->PopClipRect();
}

void SceneEditorView::AppendColliderDebugLines(std::vector<DebugLineVertex> &out) {
    if (!context_) return;
    auto *sceneObjectCollider = context_->GetComponent<SceneObjectCollider>();
    if (!sceneObjectCollider) return;

    constexpr Vector4 kSolidColor{ 0.31f, 0.90f, 0.47f, 1.0f };
    constexpr Vector4 kTriggerColor{ 1.0f, 0.78f, 0.16f, 1.0f };
    constexpr Vector4 kRayColor{ 1.0f, 0.35f, 0.86f, 1.0f };

    for (auto *collider : sceneObjectCollider->GetRegisteredColliders()) {
        if (!collider || !collider->IsActive()) continue;
        const Vector4 &color = collider->IsTrigger() ? kTriggerColor : kSolidColor;

        if (collider->Is2D()) {
            if (auto info = collider->BuildColliderInfo2D()) {
                AppendCollider2DShape(out, *info, collider->GetOwnerWorldPosition().z, color);
            }
            continue;
        }

        if (auto info = collider->BuildColliderInfo3D()) {
            const Quaternion rotation = collider->GetSyncedOwnerRotation();
            std::visit([&](const auto &shape) {
                using T = std::decay_t<decltype(shape)>;
                if constexpr (std::is_same_v<T, ColliderInfo3D::SphereShape3D>) {
                    AppendWireSphere3D(out, shape.center, shape.radius, color);
                } else if constexpr (std::is_same_v<T, ColliderInfo3D::BoxShape3D>) {
                    AppendWireBox3D(out, shape.center, shape.halfExtents, rotation, color);
                } else if constexpr (std::is_same_v<T, ColliderInfo3D::CapsuleShape3D>) {
                    AppendWireCapsule3D(out, shape.center, shape.radius, shape.height, rotation, color);
                }
                // ConvexMesh/ConcaveMesh/HeightFieldはエンジン内に生成コンポーネントが無いため未対応
            }, info->shape);
        } else if (auto *rayCollider = dynamic_cast<RayCollider *>(collider)) {
            // RayColliderは常駐形状を持たないため、方向・距離からレイとして描画する
            const Vector3 rotatedDirection = collider->GetSyncedOwnerRotation().RotateVector(rayCollider->GetDirection());
            AppendRayGizmo(out, collider->GetSyncedOwnerPosition(), rotatedDirection, rayCollider->GetMaxDistance(), kRayColor);
        }
    }
}

void SceneEditorView::AppendWireBox3D(std::vector<DebugLineVertex> &out, const Vector3 &center, const Vector3 &halfExtents, const Quaternion &rotation, const Vector4 &color) {
    Vector3 corners[8];
    for (int i = 0; i < 8; ++i) {
        const Vector3 localOffset(
            (i & 1) ? halfExtents.x : -halfExtents.x,
            (i & 2) ? halfExtents.y : -halfExtents.y,
            (i & 4) ? halfExtents.z : -halfExtents.z);
        corners[i] = center + rotation.RotateVector(localOffset);
    }

    static constexpr int kEdges[12][2] = {
        {0, 1}, {0, 2}, {0, 4}, {1, 3}, {1, 5}, {2, 3},
        {2, 6}, {3, 7}, {4, 5}, {4, 6}, {5, 7}, {6, 7},
    };
    for (const auto &edge : kEdges) {
        out.push_back({ corners[edge[0]], color });
        out.push_back({ corners[edge[1]], color });
    }
}

void SceneEditorView::AppendWireSphere3D(std::vector<DebugLineVertex> &out, const Vector3 &center, float radius, const Vector4 &color) {
    constexpr int kSegments = 24;

    // XY・XZ・YZの3つの円で球を近似する
    for (int plane = 0; plane < 3; ++plane) {
        Vector3 prevPoint{};
        bool hasPrev = false;
        for (int i = 0; i <= kSegments; ++i) {
            const float angle = static_cast<float>(i) / static_cast<float>(kSegments) * 2.0f * std::numbers::pi_v<float>;
            const float c = std::cos(angle) * radius;
            const float s = std::sin(angle) * radius;
            Vector3 point = center;
            if (plane == 0) { point.x += c; point.y += s; }
            else if (plane == 1) { point.x += c; point.z += s; }
            else { point.y += c; point.z += s; }

            if (hasPrev) {
                out.push_back({ prevPoint, color });
                out.push_back({ point, color });
            }
            prevPoint = point;
            hasPrev = true;
        }
    }
}

void SceneEditorView::AppendWireCapsule3D(std::vector<DebugLineVertex> &out, const Vector3 &center, float radius, float height, const Quaternion &rotation, const Vector4 &color) {
    constexpr int kSegments = 24;
    const float halfHeight = height * 0.5f;
    const Vector3 topCenter = center + rotation.RotateVector(Vector3(0.0f, halfHeight, 0.0f));
    const Vector3 bottomCenter = center - rotation.RotateVector(Vector3(0.0f, halfHeight, 0.0f));

    // カプセルはReactPhysics3Dの規約に合わせてY軸方向を軸とする（半球部は簡略化して円柱部のみ描画する）
    // 円周上の点はローカルXZ平面で計算してからコライダーの回転を適用する
    auto appendCircleXZ = [&](const Vector3 &circleCenter) {
        Vector3 prevPoint{};
        bool hasPrev = false;
        for (int i = 0; i <= kSegments; ++i) {
            const float angle = static_cast<float>(i) / static_cast<float>(kSegments) * 2.0f * std::numbers::pi_v<float>;
            const Vector3 localOffset(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius);
            const Vector3 point = circleCenter + rotation.RotateVector(localOffset);
            if (hasPrev) {
                out.push_back({ prevPoint, color });
                out.push_back({ point, color });
            }
            prevPoint = point;
            hasPrev = true;
        }
    };
    appendCircleXZ(topCenter);
    appendCircleXZ(bottomCenter);

    for (int i = 0; i < 4; ++i) {
        const float angle = static_cast<float>(i) / 4.0f * 2.0f * std::numbers::pi_v<float>;
        const Vector3 localOffset(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius);
        const Vector3 rotatedOffset = rotation.RotateVector(localOffset);
        out.push_back({ topCenter + rotatedOffset, color });
        out.push_back({ bottomCenter + rotatedOffset, color });
    }
}

void SceneEditorView::AppendCollider2DShape(std::vector<DebugLineVertex> &out, const ColliderInfo2D &info, float worldZ, const Vector4 &color) {
    std::visit([&](const auto &shape) {
        using T = std::decay_t<decltype(shape)>;
        if constexpr (std::is_same_v<T, Math::Point2D>) {
            // 点は微小な十字線として表現する
            const Vector3 p(shape.position.x, shape.position.y, worldZ);
            constexpr float kSize = 0.1f;
            out.push_back({ p - Vector3(kSize, 0.0f, 0.0f), color });
            out.push_back({ p + Vector3(kSize, 0.0f, 0.0f), color });
            out.push_back({ p - Vector3(0.0f, kSize, 0.0f), color });
            out.push_back({ p + Vector3(0.0f, kSize, 0.0f), color });
        } else if constexpr (std::is_same_v<T, Math::Circle>) {
            constexpr int kSegments = 24;
            Vector3 prevPoint{};
            bool hasPrev = false;
            for (int i = 0; i <= kSegments; ++i) {
                const float angle = static_cast<float>(i) / static_cast<float>(kSegments) * 2.0f * std::numbers::pi_v<float>;
                const Vector3 point(shape.center.x + std::cos(angle) * shape.radius, shape.center.y + std::sin(angle) * shape.radius, worldZ);
                if (hasPrev) {
                    out.push_back({ prevPoint, color });
                    out.push_back({ point, color });
                }
                prevPoint = point;
                hasPrev = true;
            }
        } else if constexpr (std::is_same_v<T, Math::Rect>) {
            const float c = std::cos(shape.rotation);
            const float s = std::sin(shape.rotation);
            const Vector2 ex{ shape.halfSize.x * c, shape.halfSize.x * s };
            const Vector2 ey{ -shape.halfSize.y * s, shape.halfSize.y * c };
            const Vector2 localCorners[4] = {
                shape.center - ex - ey,
                shape.center + ex - ey,
                shape.center + ex + ey,
                shape.center - ex + ey,
            };
            const Vector3 corners[4] = {
                Vector3(localCorners[0].x, localCorners[0].y, worldZ),
                Vector3(localCorners[1].x, localCorners[1].y, worldZ),
                Vector3(localCorners[2].x, localCorners[2].y, worldZ),
                Vector3(localCorners[3].x, localCorners[3].y, worldZ),
            };
            for (int i = 0; i < 4; ++i) {
                const int next = (i + 1) % 4;
                out.push_back({ corners[i], color });
                out.push_back({ corners[next], color });
            }
        } else if constexpr (std::is_same_v<T, Math::Segment2D>) {
            out.push_back({ Vector3(shape.start.x, shape.start.y, worldZ), color });
            out.push_back({ Vector3(shape.end.x, shape.end.y, worldZ), color });
        } else if constexpr (std::is_same_v<T, Math::Capsule2D>) {
            const Vector2 axis = shape.end - shape.start;
            const float axisLength = axis.Length();
            const Vector2 dir = axisLength > 1e-6f ? axis * (1.0f / axisLength) : Vector2(1.0f, 0.0f);
            const Vector2 offset = dir.Perpendicular() * shape.radius;

            const Vector3 s1(shape.start.x + offset.x, shape.start.y + offset.y, worldZ);
            const Vector3 s2(shape.start.x - offset.x, shape.start.y - offset.y, worldZ);
            const Vector3 e1(shape.end.x + offset.x, shape.end.y + offset.y, worldZ);
            const Vector3 e2(shape.end.x - offset.x, shape.end.y - offset.y, worldZ);
            out.push_back({ s1, color });
            out.push_back({ e1, color });
            out.push_back({ s2, color });
            out.push_back({ e2, color });

            // 端の半円（近似）
            const float baseAngle = std::atan2(dir.y, dir.x);
            auto appendArc = [&](const Vector2 &arcCenter, float startAngle) {
                constexpr int kArcSegments = 12;
                Vector3 prevPoint{};
                bool hasPrev = false;
                for (int i = 0; i <= kArcSegments; ++i) {
                    const float angle = startAngle + static_cast<float>(i) / static_cast<float>(kArcSegments) * std::numbers::pi_v<float>;
                    const Vector3 point(arcCenter.x + std::cos(angle) * shape.radius, arcCenter.y + std::sin(angle) * shape.radius, worldZ);
                    if (hasPrev) {
                        out.push_back({ prevPoint, color });
                        out.push_back({ point, color });
                    }
                    prevPoint = point;
                    hasPrev = true;
                }
            };
            appendArc(shape.end, baseAngle - std::numbers::pi_v<float> * 0.5f);
            appendArc(shape.start, baseAngle + std::numbers::pi_v<float> * 0.5f);
        }
    }, info.shape);
}

void SceneEditorView::AppendRayGizmo(std::vector<DebugLineVertex> &out, const Vector3 &origin, const Vector3 &direction, float length, const Vector4 &color) {
    const Vector3 dir = direction.Length() > 1e-6f ? direction.Normalize() : Vector3(0.0f, -1.0f, 0.0f);
    const Vector3 end = origin + dir * length;
    out.push_back({ origin, color });
    out.push_back({ end, color });
}

void SceneEditorView::ApplyWorldMatrixToTransform(Transform *transform, const Matrix4x4 &worldMatrix) {
    if (!transform) return;

    // 親がいる場合はローカル行列へ変換してから適用する
    Matrix4x4 local = worldMatrix;
    if (auto *parentObject = transform->GetParentObject()) {
        if (auto *parentTransform = parentObject->GetComponent<Transform>()) {
            local = worldMatrix * parentTransform->GetWorldMatrix().Inverse();
        }
    }

    Vector3 translate;
    Vector3 rotateDeg;
    Vector3 scale;
    ImGuizmo::DecomposeMatrixToComponents(&local.m[0][0], &translate.x, &rotateDeg.x, &scale.x);

    // オイラー角を経由すると（ジンバルロックや ImGuizmo との回転順の差により）
    // ドラッグ中に回転が不安定になるため、回転はローカル行列から直接クォータニオンへ変換する。
    // （translate/scale の抽出には ImGuizmo の分解結果をそのまま使う）
    Matrix4x4 rotationOnly = local;
    auto normalizeRow = [](Matrix4x4 &m, int row, float scaleValue) {
        if (std::abs(scaleValue) < 1e-8f) return;
        m.m[row][0] /= scaleValue;
        m.m[row][1] /= scaleValue;
        m.m[row][2] /= scaleValue;
    };
    normalizeRow(rotationOnly, 0, scale.x);
    normalizeRow(rotationOnly, 1, scale.y);
    normalizeRow(rotationOnly, 2, scale.z);

    transform->SetTranslate(translate);
    transform->SetRotateQuaternion(Quaternion::MakeFromRotationMatrix(rotationOnly));
    transform->SetScale(scale);
}

void SceneEditorView::ShowGizmo(const std::unordered_set<EmptyObject *> &selectedObjects, SceneEditorCommands *commands, const ImVec2 &imagePos, const ImVec2 &imageSize) {
    // 選択集合が変わった場合は編集状態をリセットする
    if (selectedObjects != gizmoTargetObjects_) {
        gizmoTargetObjects_ = selectedObjects;
        isGizmoEditing_ = false;
    }

    // 選択群内の祖先が動けば子孫は階層継承で自動的に追従するため、
    // 子孫にも同じデルタを直接適用すると二重に移動してしまう（ドラッグ中は毎フレーム累積し発散する）。
    // 選択群内に祖先を持つオブジェクトはギズモの直接適用対象から除外する
    const auto isDescendantOfSelection = [this](EmptyObject *obj) {
        auto *transform = obj->GetComponent<Transform>();
        EmptyObject *parent = transform ? transform->GetParentObject() : nullptr;
        while (parent) {
            if (gizmoTargetObjects_.contains(parent)) return true;
            auto *parentTransform = parent->GetComponent<Transform>();
            parent = parentTransform ? parentTransform->GetParentObject() : nullptr;
        }
        return false;
    };

    std::vector<std::pair<EmptyObject *, Transform *>> targets;
    for (auto *obj : gizmoTargetObjects_) {
        if (!obj) continue;
        if (isDescendantOfSelection(obj)) continue;
        if (auto *transform = obj->GetComponent<Transform>()) {
            targets.emplace_back(obj, transform);
        }
    }
    if (targets.empty() || imageSize.x <= 0.0f || imageSize.y <= 0.0f) return;

    // 「2D」表示モード中は専用の正射影パン・ズームカメラ（view2D_/projection2D_）を使う。
    // ImGuizmoは正射影投影にも対応しており、RaycastSceneMeshes同様、判定ロジック自体は共通で流用できる
    const bool isTwoDMode = (displayMode_ == SceneRenderer::EditorDisplayMode::TwoDOnly);
    ImGuizmo::SetOrthographic(isTwoDMode);
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(imagePos.x, imagePos.y, imageSize.x, imageSize.y);

    Matrix4x4 view = GetActiveView();
    Matrix4x4 projection = GetActiveProjection();
    const bool isSingle = (targets.size() == 1);

    // 選択中にSpriteRenderer（2Dオブジェクト）が1つでも含まれる場合、回転操作をZ軸のみに制限する。
    // SpriteRendererはXY平面上に貼り付く板ポリゴンのため、X/Y軸回転は板を真横から見る形になり
    // 実用上ほぼ意味がなく、誤操作で意図せず板が傾いてしまいやすい
    bool hasSpriteRendererInSelection = false;
    for (auto &[obj, transform] : targets) {
        if (obj->GetComponent<SpriteRenderer>()) {
            hasSpriteRendererInSelection = true;
            break;
        }
    }
    ImGuizmo::OPERATION effectiveOperation = static_cast<ImGuizmo::OPERATION>(gizmoOperation_);
    if (effectiveOperation == ImGuizmo::ROTATE && hasSpriteRendererInSelection) {
        effectiveOperation = ImGuizmo::ROTATE_Z;
    }

    if (!ImGuizmo::IsUsing()) {
        // 未操作中はギズモの基準行列を毎フレーム最新の状態から作り直す。
        // 単一選択時は対象自身のワールド行列（Local/Worldモードの切り替えが意味を持つ）、
        // 複数選択時は選択群の平均位置を中心としたワールド軸固定の仮想行列にする
        // （バラバラな向きのオブジェクト群に共通のローカル基準は定義できないため）。
        if (isSingle) {
            groupGizmoMatrix_ = targets[0].second->GetWorldMatrix();
        } else {
            Vector3 pivot{ 0.0f, 0.0f, 0.0f };
            for (auto &[obj, transform] : targets) {
                const Matrix4x4 &world = transform->GetWorldMatrix();
                pivot = pivot + Vector3(world.m[3][0], world.m[3][1], world.m[3][2]);
            }
            pivot = pivot * (1.0f / static_cast<float>(targets.size()));
            groupGizmoMatrix_ = Matrix4x4::Identity();
            groupGizmoMatrix_.m[3][0] = pivot.x;
            groupGizmoMatrix_.m[3][1] = pivot.y;
            groupGizmoMatrix_.m[3][2] = pivot.z;
        }
    }

    // このフレームの増分（デルタ）を算出するため、Manipulate直前の状態を控えておく
    const Matrix4x4 beforeManipulate = groupGizmoMatrix_;

    // グリッドスナップ（オブジェクトのグリッド配置）。Ctrlキー押下中はトグル設定を一時的に反転する
    // （Unity等のシーンビューと同じ操作感）。ImGuizmoは操作種別に応じてsnapの意味が変わる
    // （TRANSLATE/SCALEはXYZ各軸の間隔、ROTATE/ROTATE_Zはsnap.xのみを角度[度]として使う）
    const bool useSnap = gizmoSnapEnabled_ != ImGui::IsKeyDown(ImGuiMod_Ctrl);
    float snap[3] = { gizmoSnapTranslate_, gizmoSnapTranslate_, gizmoSnapTranslate_ };
    if (effectiveOperation == ImGuizmo::ROTATE || effectiveOperation == ImGuizmo::ROTATE_Z) {
        snap[0] = gizmoSnapRotateDegrees_;
    } else if (effectiveOperation == ImGuizmo::SCALE) {
        snap[0] = snap[1] = snap[2] = gizmoSnapScale_;
    }

    ImGuizmo::Manipulate(
        &view.m[0][0], &projection.m[0][0],
        effectiveOperation,
        isSingle ? static_cast<ImGuizmo::MODE>(gizmoMode_) : ImGuizmo::WORLD,
        &groupGizmoMatrix_.m[0][0],
        nullptr,
        useSnap ? snap : nullptr);

    if (ImGuizmo::IsUsing()) {
        // 操作開始時に変更前の状態を全対象分保存する（Undo用）
        if (!isGizmoEditing_) {
            isGizmoEditing_ = true;
            gizmoBeforeStates_.clear();
            for (auto &[obj, transform] : targets) {
                gizmoBeforeStates_.push_back({ obj, obj->SaveComponentToJson(transform) });
            }
        }

        // このフレームでギズモに加えられた増分（ワールド空間）を全対象へ均等に適用する
        // （row-vector規約: A' = A * D なら D = A^-1 * A'）
        const Matrix4x4 delta = beforeManipulate.Inverse() * groupGizmoMatrix_;
        for (auto &[obj, transform] : targets) {
            const Matrix4x4 newWorld = transform->GetWorldMatrix() * delta;
            ApplyWorldMatrixToTransform(transform, newWorld);
        }
    } else if (isGizmoEditing_) {
        // 操作終了時にUndo/Redo履歴へ積む（全対象をまとめて1操作にする）
        isGizmoEditing_ = false;
        if (commands) {
            auto composite = std::make_unique<CompositeCommand>(
                (gizmoBeforeStates_.size() > 1) ? (Translation("editor.command.transform.prefix") + std::to_string(gizmoBeforeStates_.size()) + Translation("editor.command.objects.suffix")) : Translation("editor.command.transformobject"));
            bool anyChanged = false;
            for (auto &state : gizmoBeforeStates_) {
                if (!state.object) continue;
                auto *transform = state.object->GetComponent<Transform>();
                if (!transform) continue;
                JSON after = state.object->SaveComponentToJson(transform);
                if (after != state.before) {
                    anyChanged = true;
                    composite->AddCommand(std::make_unique<ComponentEditCommand>(state.object, transform, state.before, after));
                }
            }
            if (anyChanged) commands->PushExecuted(std::move(composite));
        }
        gizmoBeforeStates_.clear();
    }
}

} // namespace KashipanEngine

#endif // USE_IMGUI
