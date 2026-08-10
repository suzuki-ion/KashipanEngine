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
#include "Scene/Editor/EditorSettings.h"
#include "Scene/Editor/PrefabUtility.h"
#include "Scene/Editor/SceneEditorCommands.h"
#include "Scene/Editor/SceneObjectHierarchy.h"
#include "Utilities/Translation.h"
#include "Scene/Components/Render/SceneRenderer.h"
#include "Scene/Components/SceneObjectCollider.h"
#include "Graphics/ScreenBuffer.h"
#include "Graphics/Resources/ConstantBufferResource.h"
#include "Objects/EmptyObject.h"
#include "Objects/Components/MeshFilter.h"
#include "Objects/Components/Render/Camera3D.h"
#include "Objects/Components/Render/CameraRenderer.h"
#include "Objects/Components/Render/SceneViewOrbitState.h"
#include "Objects/Components/Render/Light.h"
#include "Objects/Components/Render/LightRenderer.h"
#include "Objects/Components/Render/MeshRenderer.h"
#include "Objects/Components/Render/ScreenBufferObject.h"
#include "Objects/Components/Render/SkinnedMeshRenderer.h"
#include "Objects/Components/Collider/ICollider.h"
#include "Objects/Components/Collider/RayCollider.h"
#include "Objects/Components/Transform.h"
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
    // screenBuffer_ の実体は sceneViewObject_ の ScreenBufferObject コンポーネントが所有している。
    // シーン破棄時はそちらの Finalize() が RenderTargetCarryOverRegistry 経由で後始末
    // （同名コンポーネントへの引き継ぎ、または破棄）を行うため、ここでは何もしない
    screenBuffer_ = nullptr;
    sceneViewObject_ = nullptr;
}

void SceneEditorView::ShowImGui(const std::unordered_set<EmptyObject *> &selectedObjects, SceneEditorCommands *commands, SceneObjectHierarchy *hierarchy) {
    EnsureResources();
    UpdateCameraBuffer();
    RegisterEditorTarget();
    UpdateEditorDebugDraw();
    // シーンビュー上で選択中オブジェクトへアウトラインを付ける（このシーンビュー用描画先にのみ適用される）
    if (auto *sceneRenderer = context_->GetComponent<SceneRenderer>()) {
        sceneRenderer->SetEditorSelectedObjects(selectedObjects);
    }
    ShowSceneViewWindow(selectedObjects, commands, hierarchy);
}

void SceneEditorView::EnsureResources() {
    EnsureSceneViewObject();
    if (sceneViewObject_) {
        if (auto *screenBufferObject = sceneViewObject_->GetComponent<ScreenBufferObject>()) {
            screenBuffer_ = screenBufferObject->GetScreenBuffer();
        }
    }
    if (!cameraBuffer_) {
        cameraBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(CameraConstant));
    }
}

void SceneEditorView::EnsureSceneViewObject() {
    sceneViewObject_ = nullptr;
    if (!context_) return;

    // 生ポインタをフレームをまたいでキャッシュせず、毎回UUIDから引き直す
    // （Play開始時のDeleteEditorOnlyObjectsで対象が削除され得るため、CameraRenderer::GetTargetObject
    // と同じ方式でダングリングポインタ参照を避ける）
    if (sceneViewObjectID_.IsValid()) {
        sceneViewObject_ = context_->GetSceneObject(sceneViewObjectID_);
        if (sceneViewObject_) return;
    }

    for (auto *obj : context_->GetSceneObjects()) {
        if (!obj || !obj->IsEditorOnly()) continue;
        if (obj->GetComponent<Camera3D>() && obj->GetComponent<ScreenBufferObject>()) {
            sceneViewObject_ = obj;
            break;
        }
    }

    if (sceneViewObject_) {
        sceneViewObjectID_ = sceneViewObject_->GetObjectID();
        // 既存オブジェクトが見つかった場合は、そのTransformからオービット操作の起点
        // （yaw_/pitch_/target_）を逆算する。distance_はTransformに保存されない値のため
        // SceneViewOrbitStateコンポーネントに保存しておいた値を使う（無ければ付与する）ことで、
        // target_ = position + forward * distance_ によりカメラの位置・向き・距離を完全に再現できる
        if (auto *transform = sceneViewObject_->GetComponent<Transform>()) {
            auto *orbitState = sceneViewObject_->GetComponent<SceneViewOrbitState>();
            if (!orbitState) orbitState = sceneViewObject_->AddComponent<SceneViewOrbitState>();
            const Vector3 forward = transform->GetRotateQuaternion().RotateVector(Vector3(0.0f, 0.0f, 1.0f));
            yaw_ = std::atan2(forward.x, forward.z);
            pitch_ = std::clamp(std::asin(std::clamp(-forward.y, -1.0f, 1.0f)), -1.55f, 1.55f);
            distance_ = orbitState->GetDistance();
            target_ = transform->GetTranslate() + forward * distance_;
        }
        return;
    }

    // シーン上に見つからない場合は新規作成する。ScreenBufferObjectの内部バッファ自動生成と
    // 同様のブートストラップ処理のため、Undo/Redoコマンドは通さない
    EmptyObject *newObj = context_->CreateEmptyObject("Scene View");
    if (!newObj) return;
    newObj->SetEditorOnly(true);
    newObj->AddComponent<Camera3D>();
    newObj->AddComponent<SceneViewOrbitState>();
    if (auto *screenBufferObject = newObj->AddComponent<ScreenBufferObject>()) {
        screenBufferObject->SetName("EditorSceneView");
    }
    sceneViewObject_ = newObj;
    sceneViewObjectID_ = newObj->GetObjectID();
    // 新規作成時は既存の既定値（target(0,0,0)・distance 10・yaw 0・pitch 0.3）をそのまま使う
}

void SceneEditorView::UpdateCameraBuffer() {
    if (!cameraBuffer_ || !screenBuffer_ || !sceneViewObject_) return;
    auto *transform = sceneViewObject_->GetComponent<Transform>();
    auto *camera3d = sceneViewObject_->GetComponent<Camera3D>();
    if (!transform || !camera3d) return;

    // ピッチ→ヨーの順で回転（行ベクトル規約）
    Matrix4x4 rotateX;
    rotateX.MakeRotateX(pitch_);
    Matrix4x4 rotateY;
    rotateY.MakeRotateY(yaw_);
    const Matrix4x4 rotation = rotateX * rotateY;

    const Vector3 forward(rotation.m[2][0], rotation.m[2][1], rotation.m[2][2]);
    const Vector3 eye = target_ - forward * distance_;

    // 算出した位置・向きをTransformへ書き戻す（シーン保存時にそのまま永続化される）
    transform->SetTranslate(eye);
    transform->SetRotateQuaternion(Quaternion::MakeFromRotationMatrix(rotation));
    // distance_はTransformに残らないため、SceneViewOrbitStateへ書き戻して永続化する
    if (auto *orbitState = sceneViewObject_->GetComponent<SceneViewOrbitState>()) {
        orbitState->SetDistance(distance_);
    }

    view_ = transform->GetWorldMatrix().Inverse();
    const float width = static_cast<float>(screenBuffer_->GetWidth());
    const float height = static_cast<float>(screenBuffer_->GetHeight());
    const float aspect = (height > 0.0f) ? (width / height) : (16.0f / 9.0f);
    camera3d->SetAspectRatio(aspect);
    projection_.MakePerspectiveFovMatrix(camera3d->GetFovY(), aspect, camera3d->GetNearClip(), camera3d->GetFarClip());

    CameraConstant constant{};
    constant.view = view_;
    constant.projection = projection_;
    constant.viewProjection = view_ * projection_;
    constant.eyePosition = Vector4(eye.x, eye.y, eye.z, 1.0f);
    constant.fov = camera3d->GetFovY();
    cameraEye_ = eye;

    if (void *mapped = cameraBuffer_->Map()) {
        std::memcpy(mapped, &constant, sizeof(constant));
    }
}

void SceneEditorView::RegisterEditorTarget() {
    if (!context_) return;
    auto *sceneRenderer = context_->GetComponent<SceneRenderer>();
    if (sceneRenderer && screenBuffer_) {
        auto *camera3d = sceneViewObject_ ? sceneViewObject_->GetComponent<Camera3D>() : nullptr;
        // シャドウマップのカスケード計算用にエディターカメラの情報も渡す
        SceneRenderer::EditorCameraInfo cameraInfo;
        cameraInfo.viewProjection = view_ * projection_;
        cameraInfo.position = cameraEye_;
        cameraInfo.nearClip = camera3d ? camera3d->GetNearClip() : 0.1f;
        cameraInfo.farClip = camera3d ? camera3d->GetFarClip() : 1000.0f;
        cameraInfo.valid = true;
        // オーナーオブジェクトも渡しておくことで、GetTargetOwner経由でこのScreenBufferへの
        // ポストエフェクト適用対象を「Scene View」オブジェクトへ絞り込めるようにする
        // （未指定のままだとRenderPostProcessがオーナー無しとして処理を打ち切ってしまい、
        // シーンビュー上でポストエフェクトが一切適用されなくなる）
        sceneRenderer->SetEditorTarget(screenBuffer_, cameraBuffer_.get(), cameraInfo, sceneViewObject_);
    }
}

void SceneEditorView::ShowSceneViewWindow(const std::unordered_set<EmptyObject *> &selectedObjects, SceneEditorCommands *commands, SceneObjectHierarchy *hierarchy) {
    if (!ImGui::Begin(TranslationLabel("editor.sceneview.window"))) {
        ImGui::End();
        return;
    }

    // シーンビューにフォーカスがある間も、ヒエラルキーと同じショートカット
    // （Ctrl+C/Ctrl+V/Ctrl+Shift+V/Ctrl+D）で選択中オブジェクトを操作できるようにする
    if (hierarchy) hierarchy->HandleKeyboardShortcuts();

    //--------- ギズモ操作の切り替えツールバー ---------//
    if (ImGui::RadioButton(TranslationLabel("editor.sceneview.gizmo.translate"), gizmoOperation_ == ImGuizmo::TRANSLATE)) gizmoOperation_ = ImGuizmo::TRANSLATE;
    ImGui::SameLine();
    if (ImGui::RadioButton(TranslationLabel("editor.sceneview.gizmo.rotate"), gizmoOperation_ == ImGuizmo::ROTATE)) gizmoOperation_ = ImGuizmo::ROTATE;
    ImGui::SameLine();
    if (ImGui::RadioButton(TranslationLabel("editor.sceneview.gizmo.scale"), gizmoOperation_ == ImGuizmo::SCALE)) gizmoOperation_ = ImGuizmo::SCALE;
    ImGui::SameLine();
    if (ImGui::RadioButton(TranslationLabel("editor.sceneview.gizmo.local"), gizmoMode_ == ImGuizmo::LOCAL)) gizmoMode_ = ImGuizmo::LOCAL;
    ImGui::SameLine();
    if (ImGui::RadioButton(TranslationLabel("editor.sceneview.gizmo.world"), gizmoMode_ == ImGuizmo::WORLD)) gizmoMode_ = ImGuizmo::WORLD;

    //--------- デバッグ表示の有効/無効切り替え ---------//
    if (ImGui::Checkbox(TranslationLabel("editor.sceneview.show.grid"), &showGrid_)) EditorSettings::SetBool("sceneView.showGrid", showGrid_);
    ImGui::SameLine();
    if (ImGui::Checkbox(TranslationLabel("editor.sceneview.show.lights"), &showLightMarkers_)) EditorSettings::SetBool("sceneView.showLightMarkers", showLightMarkers_);
    ImGui::SameLine();
    if (ImGui::Checkbox(TranslationLabel("editor.sceneview.show.cameras"), &showCameraMarkers_)) EditorSettings::SetBool("sceneView.showCameraMarkers", showCameraMarkers_);
    ImGui::SameLine();
    if (ImGui::Checkbox(TranslationLabel("editor.sceneview.show.colliders"), &showColliderGizmos_)) EditorSettings::SetBool("sceneView.showColliderGizmos", showColliderGizmos_);
    ImGui::SameLine();
    if (ImGui::Checkbox(TranslationLabel("editor.sceneview.show.bones"), &showBoneGizmos_)) EditorSettings::SetBool("sceneView.showBoneGizmos", showBoneGizmos_);

    //--------- 背景設定（単色 or テクスチャ） ---------//
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
            if (auto *screenBufferObject = sceneViewObject_ ? sceneViewObject_->GetComponent<ScreenBufferObject>() : nullptr) {
                screenBufferObject->SetSize(newWidth, newHeight);
            }
        }
    }
    const ImVec2 drawSize = avail;
    const ImVec2 imagePos = ImGui::GetCursorScreenPos();
    ImGui::Image(static_cast<ImTextureID>(screenBuffer_->GetPreviewSrvHandle().ptr), drawSize);

    // Assetsウィンドウからのプレハブファイル（.prefab）のドラッグ&ドロップ。ドラッグ中は毎フレーム
    // カーソル直下の配置予定位置（シーン上のメッシュ表面、無ければ地面平面）へ半透明のプレビューを
    // 表示し、Unity等と同様、実際にドロップされた瞬間にそのままシーンへ配置する
    HandlePrefabDragDrop(hierarchy, imagePos, drawSize);

    //--------- カメラ操作（画像上でのマウス操作） ---------//
    HandleCameraInput();

    //--------- クリックによるオブジェクト選択 ---------//
    HandleObjectPicking(hierarchy, imagePos, drawSize);

    // グリッド線・当たり判定のワイヤーフレームは screenBuffer_ へGPUで直接描画される
    // （UpdateEditorDebugDraw で設定済み。DebugGrid/DebugLinesパイプライン参照）

    //--------- ライトのデバッグ表示 ---------//
    if (showLightMarkers_) DrawLightMarkers(imagePos, drawSize);

    //--------- カメラのデバッグ表示 ---------//
    if (showCameraMarkers_) DrawCameraMarkers(imagePos, drawSize);

    //--------- ImGuizmo によるギズモ表示 ---------//
    ShowGizmo(selectedObjects, commands, imagePos, drawSize);

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
    ImGuiIO &io = ImGui::GetIO();

    // ホイールでズーム
    if (io.MouseWheel != 0.0f) {
        distance_ *= std::pow(0.9f, io.MouseWheel);
        distance_ = std::clamp(distance_, 0.1f, 10000.0f);
    }
    // 右ドラッグで回転
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

void SceneEditorView::UpdateEditorDebugDraw() {
    if (!context_) return;
    auto *sceneRenderer = context_->GetComponent<SceneRenderer>();
    if (!sceneRenderer) return;

    EditorDebugDrawSettings settings;
    settings.showGrid = showGrid_;
    // カメラのズーム量に応じてグリッドの表示範囲を追従させる（Blenderのように「無限」に感じられる範囲を保つ）
    settings.gridFadeDistance = std::clamp(distance_ * 4.0f, 20.0f, 2000.0f);

    settings.backgroundColor = backgroundColor_;
    settings.backgroundTextureHandle = backgroundTexturePath_.empty()
        ? TextureManager::kInvalidHandle
        : TextureManager::GetTextureFromAssetPath(backgroundTexturePath_);

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

    sceneRenderer->SetEditorDebugDraw(std::move(settings));
}

bool SceneEditorView::ProjectToImage(const Vector3 &worldPosition, const ImVec2 &imagePos, const ImVec2 &imageSize, ImVec2 &outScreenPos, bool clampToVisibleArea) const {
    const Matrix4x4 viewProjection = view_ * projection_;
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

    // マーカー表示が無効な種別は、画面上にアイコンが出ていないためクリック判定からも除外する
    if (showLightMarkers_) {
        for (auto *lightRenderer : sceneRenderer->GetLightRenderers()) {
            if (!lightRenderer || !lightRenderer->IsActive()) continue;
            considerIcon(lightRenderer->GetWorldPosition(), lightRenderer->GetOwnerObject());
        }
    }
    if (showCameraMarkers_) {
        for (auto *cameraRenderer : sceneRenderer->GetCameraRenderers()) {
            if (!cameraRenderer || !cameraRenderer->IsActive()) continue;
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
    const Matrix4x4 invViewProjection = (view_ * projection_).Inverse();
    outRayStart = UnprojectNdc(invViewProjection, ndcX, ndcY, 0.0f);
    outRayEnd = UnprojectNdc(invViewProjection, ndcX, ndcY, 1.0f);

    for (auto *obj : context_->GetSceneObjects()) {
        if (!obj || !obj->IsActive()) continue;

        // シーンビューに描画される対象（アクティブなMeshRenderer/SkinnedMeshRendererを持つ）だけを判定対象にする
        auto *meshFilter = obj->GetComponent<MeshFilter>();
        if (!meshFilter || !meshFilter->HasMesh()) continue;
        auto *meshRenderer = obj->GetComponent<MeshRenderer>();
        auto *skinnedMeshRenderer = obj->GetComponent<SkinnedMeshRenderer>();
        const bool hasVisibleRenderer =
            (meshRenderer && meshRenderer->IsActive()) ||
            (skinnedMeshRenderer && skinnedMeshRenderer->IsActive());
        if (!hasVisibleRenderer) continue;

        auto *transform = obj->GetComponent<Transform>();
        const Matrix4x4 world = transform ? transform->GetWorldMatrix() : Matrix4x4::Identity();

        // レイをオブジェクトのローカル空間へ変換して三角形と判定する
        // （アフィン変換では線分上のパラメータtが保存されるため、tはワールド空間の奥行き比較にそのまま使える）
        const Matrix4x4 invWorld = world.Inverse();
        const Vector3 localStart = TransformPoint(outRayStart, invWorld);
        const Vector3 localDir = TransformPoint(outRayEnd, invWorld) - localStart;

        const auto &model = ModelManager::GetModelData(meshFilter->GetMeshHandle());
        const auto &vertices = model.GetVertices();
        const auto &indices = model.GetIndices();
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

void SceneEditorView::HandleObjectPicking(SceneObjectHierarchy *hierarchy, const ImVec2 &imagePos, const ImVec2 &imageSize) {
    if (!hierarchy || !context_ || imageSize.x <= 0.0f || imageSize.y <= 0.0f) return;
    // シーンビュー画像の上にマウスがある状態での、ドラッグを伴わない左クリック（離した瞬間）で選択する
    if (!ImGui::IsItemHovered()) return;
    if (!ImGui::IsMouseReleased(ImGuiMouseButton_Left)) return;
    // カメラ操作等のドラッグ後のリリースでは選択しない（Unityと同様、微小な移動はクリック扱い）
    constexpr float kClickMoveThreshold = 3.0f;
    if (ImGui::GetIO().MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] > kClickMoveThreshold * kClickMoveThreshold) return;
    // ギズモを操作している/ギズモの上をクリックした場合は選択を変えない
    if (ImGuizmo::IsUsing() || ImGuizmo::IsOver()) return;

    const ImVec2 mouse = ImGui::GetMousePos();

    // メッシュを持たないLight/Camera等は、深度テストせず常に手前に描画されるアイコンとの
    // スクリーン座標距離でクリック判定する（メッシュの三角形ピッキングより優先する）
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

    for (auto *cameraRenderer : sceneRenderer->GetCameraRenderers()) {
        if (!cameraRenderer || !cameraRenderer->IsActive()) continue;

        const Vector3 position = cameraRenderer->GetWorldPosition();
        ImVec2 center;
        if (!ProjectToImage(position, imagePos, imageSize, center)) continue;

        constexpr ImU32 color = IM_COL32(120, 200, 255, 255);

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

void SceneEditorView::AppendCameraFrustumLines(std::vector<DebugLineVertex> &out) {
    if (!context_) return;
    auto *sceneRenderer = context_->GetComponent<SceneRenderer>();
    if (!sceneRenderer) return;

    constexpr Vector4 kNearColor{ 0.47f, 0.78f, 1.0f, 1.0f };
    constexpr Vector4 kFarColor{ 0.47f, 0.78f, 1.0f, 0.6f };

    for (auto *cameraRenderer : sceneRenderer->GetCameraRenderers()) {
        if (!cameraRenderer || !cameraRenderer->IsActive()) continue;

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

    std::vector<std::pair<EmptyObject *, Transform *>> targets;
    for (auto *obj : gizmoTargetObjects_) {
        if (!obj) continue;
        if (auto *transform = obj->GetComponent<Transform>()) {
            targets.emplace_back(obj, transform);
        }
    }
    if (targets.empty() || imageSize.x <= 0.0f || imageSize.y <= 0.0f) return;

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(imagePos.x, imagePos.y, imageSize.x, imageSize.y);

    Matrix4x4 view = view_;
    Matrix4x4 projection = projection_;
    const bool isSingle = (targets.size() == 1);

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

    ImGuizmo::Manipulate(
        &view.m[0][0], &projection.m[0][0],
        static_cast<ImGuizmo::OPERATION>(gizmoOperation_),
        isSingle ? static_cast<ImGuizmo::MODE>(gizmoMode_) : ImGuizmo::WORLD,
        &groupGizmoMatrix_.m[0][0]);

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
