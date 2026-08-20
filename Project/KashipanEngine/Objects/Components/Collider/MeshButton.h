#pragma once
#include <algorithm>
#include <cmath>
#include <memory>

#include "Objects/ObjectComponentHeader.h"
#include "Assets/ModelManager.h"
#include "Core/Window.h"
#include "Input/Input.h"
#include "Input/Mouse.h"
#include "Input/MouseButton.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector3.h"
#include "Objects/Components/MeshFilter.h"
#include "Objects/Components/Render/Camera3D.h"
#include "Objects/Components/Render/IWindowObjectComponent.h"
#include "Objects/Components/Render/MeshRenderer.h"
#include "Objects/Components/Render/NormalWindowObject.h"
#include "Objects/Components/Render/OverlayWindowObject.h"
#include "Objects/Components/Render/SkinnedMeshRenderer.h"
#include "Objects/Components/Transform.h"
#include "Utilities/UUID128.h"
#if defined(USE_IMGUI)
#include "Objects/Components/Render/TargetObjectSelector.h"
#include "Utilities/Translation.h"
#endif

namespace KashipanEngine {

/// @brief 3Dオブジェクト（MeshRenderer/SkinnedMeshRenderer）に対するマウスのホバー・押下・
///        クリック判定を行うコンポーネント（UIButtonの3D版）
/// @details 見た目の描画は一切行わない、いわば「透明な当たり判定レイヤー」。
///          同オブジェクトにMeshRendererまたはSkinnedMeshRendererがあれば（MeshRendererを優先）、
///          同じくMeshFilterが持つメッシュのローカル空間バウンディングボックスへ、マウス位置から
///          表示カメラ（Camera3D）を通したレイを飛ばして交差判定する。SkinnedMeshRendererは常に
///          このバウンディングボックス判定のみで、ボーン変形後の見た目には追従しない
///          （GPU側でスキニングした結果はCPUから直接読めないため。詳細はGetPreciseMeshTestの説明を参照）。
///          MeshRendererに限り、SetPreciseMeshTest(true)でメッシュの三角形との正確な交差判定へ
///          切り替えられる（バウンディングボックスでの絞り込みの後に行う）。
///          両方とも無い場合は常に判定falseを返す。
///          描画先ウィンドウは同オブジェクトのMeshRenderer/SkinnedMeshRenderer側（SetTargetObject）
///          で指定されたものをそのまま使うため、このコンポーネント自体はウィンドウの参照を持たない。
///          マウス座標変換の基準にするCamera3Dのみ、SetDisplayCameraObjectで別途指定する
///          （UIButton/ScreenBufferViewportのマウス座標変換と同じ考え方で、投影が透視になった版）。
class MeshButton final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(MeshButton, 1, )
    COMPONENT_CATEGORY("Collision")
    ~MeshButton() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<MeshButton>();
        ptr->displayCameraObjectID_ = displayCameraObjectID_;
        ptr->preciseMeshTest_ = preciseMeshTest_;
        ptr->dragAxis_ = dragAxis_;
        return ptr;
    }

    //==================================================
    // 参照設定
    //==================================================

    /// @brief マウス座標変換の基準にするCamera3Dを持つオブジェクトを設定する
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

    /// @brief メッシュの三角形との正確な交差判定を行うかどうかを設定する（既定false＝バウンディング
    ///        ボックスのみ）
    /// @details MeshRendererの場合のみ効果を持つ。SkinnedMeshRendererは、GPU側のコンピュートシェーダー
    ///          でスキニングした結果（変形後の頂点）をCPUから直接読み出す手段が無いため、trueにしても
    ///          常にバウンディングボックス判定のまま（バインドポーズの形状がTransformに追従するのみ）
    void SetPreciseMeshTest(bool enable) noexcept { preciseMeshTest_ = enable; }
    bool GetPreciseMeshTest() const noexcept { return preciseMeshTest_; }

    /// @brief 掴んで軸沿いにドラッグする操作の基準にする、オブジェクトのローカル軸を設定する（既定は+X）
    void SetDragAxis(const Vector3 &localAxis) { dragAxis_ = localAxis; }
    const Vector3 &GetDragAxis() const noexcept { return dragAxis_; }

    //==================================================
    // 状態取得
    //==================================================

    /// @brief マウスカーソルがオブジェクトの当たり判定内にあるかどうか
    bool IsHovered() const noexcept { return isHovered_; }
    /// @brief オブジェクト上で左クリックが押され、まだ離されていないかどうか
    /// @details 押下開始はオブジェクト上である必要があるが、離すまでの間はカーソルが判定外へ
    ///          出ても保持される（ドラッグして離せばキャンセル扱いになる、UIButtonと同じ挙動）
    bool IsPressed() const noexcept { return isPressed_; }
    /// @brief このフレームでクリックが確定した瞬間かどうか（オブジェクト上で押して、上で離した時のみtrue）
    bool IsClicked() const noexcept { return isClicked_; }

    /// @brief 掴んで軸沿いにドラッグしている間の、軸原点（掴んだ瞬間のオブジェクトのワールド座標）
    ///        からの符号付きオフセット（ワールド単位）を取得する
    /// @details 軸方向はSetDragAxisで指定したローカル軸を、掴んだ瞬間のオブジェクトのワールド回転で
    ///          変換したもの（ドラッグ中にオブジェクトが回転しても軸自体は掴んだ瞬間のまま固定される）。
    ///          値は現在のマウスレイと軸（無限直線）との最近接点までの距離で、範囲のクランプはしない
    ///          （呼び出し側で用途に応じてクランプ・正規化すること）。オブジェクトの矩形/ボリュームを
    ///          外れてもレイと軸の交点計算自体は破綻しないため、UIButton::TryGetLocalHoverPositionと
    ///          異なりドラッグ対象から大きく外れても値は取得できる。離した後は次に掴むまで最後の値を
    ///          保持する。一度も掴んでいない、またはカメラ視線とドラッグ軸がほぼ平行で計算が不安定な
    ///          場合はfalseを返す（outOffsetは変更しない）
    bool TryGetAxisDragOffset(float &outOffset) const {
        if (!hasValidDragOffset_) return false;
        outOffset = lastDragOffset_;
        return true;
    }

protected:
    void Update() override {
        isClicked_ = false;
        const bool wasPressed = isPressed_;

        const bool hoveredNow = ComputeIsHovered();
        isHovered_ = hoveredNow;

        auto *sceneContext = GetOwnerSceneContext();
        Input *input = sceneContext ? sceneContext->GetInput() : nullptr;
        if (!input) {
            isPressed_ = false;
        } else {
            Mouse &mouse = input->GetMouse();
            constexpr int kLeftButton = static_cast<int>(MouseButton::Left);

            if (!isPressed_) {
                if (hoveredNow && mouse.IsButtonTrigger(kLeftButton)) {
                    isPressed_ = true;
                    CaptureDragAxisReference();
                }
            } else if (mouse.IsButtonRelease(kLeftButton)) {
                if (hoveredNow) isClicked_ = true;
                isPressed_ = false;
            }
        }

        // 掴んでいる間（離した瞬間のこのフレームまで）は毎フレームオフセットを更新する。
        // 離した後は次に掴むまで最後の値を保持したままにする
        if ((wasPressed || isPressed_) && hasDragAxisReference_) {
            Vector3 rayWorldStart, rayWorldEnd;
            if (ComputeMouseWorldRay(rayWorldStart, rayWorldEnd)) {
                float offset = 0.0f;
                if (ComputeAxisDragOffset(rayWorldStart, rayWorldEnd - rayWorldStart, offset)) {
                    lastDragOffset_ = offset;
                    hasValidDragOffset_ = true;
                }
            }
        }
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        TargetObjectSelector::ShowSelector(TranslationLabel("component.meshbutton.display_camera"), GetOwnerSceneContext(), displayCameraObjectID_, true, false);
        ImGui::Checkbox(TranslationLabel("component.meshbutton.precise_mesh_test"), &preciseMeshTest_);
        ImGui::TextDisabled("%s", TranslationC("component.meshbutton.precise_mesh_test_desc"));
        ImGui::DragFloat3(TranslationLabel("component.meshbutton.drag_axis"), &dragAxis_.x, 0.01f);
        ImGui::TextDisabled("%s", TranslationC("component.meshbutton.drag_axis_desc"));
        ImGui::TextDisabled("%s", TranslationC("component.meshbutton.desc"));
        ImGui::Text("Hovered: %s / Pressed: %s / Clicked: %s",
            isHovered_ ? "true" : "false", isPressed_ ? "true" : "false", isClicked_ ? "true" : "false");
        if (hasValidDragOffset_) {
            ImGui::Text("Axis Drag Offset: %.3f", lastDragOffset_);
        }
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["displayCameraObjectID"] = ToJSON(displayCameraObjectID_);
        json["preciseMeshTest"] = preciseMeshTest_;
        json["dragAxis"] = ToJSON(dragAxis_);
        return json;
    }
    bool LoadFromJson(const JSON &json) override {
        displayCameraObjectID_ = json.contains("displayCameraObjectID") ? FromJSON<UUID128>(json["displayCameraObjectID"]) : UUID128();
        preciseMeshTest_ = json.value("preciseMeshTest", false);
        dragAxis_ = json.contains("dragAxis") ? FromJSON<Vector3>(json["dragAxis"]) : Vector3(1.0f, 0.0f, 0.0f);
        return true;
    }

private:
    IWindowObjectComponent *ResolveWindow(EmptyObject *targetObj) const {
        if (!targetObj) return nullptr;
        if (auto *w = targetObj->GetComponent<NormalWindowObject>()) return w;
        if (auto *w = targetObj->GetComponent<OverlayWindowObject>()) return w;
        return nullptr;
    }

    Camera3D *ResolveDisplayCamera(Transform *&outTransform) const {
        outTransform = nullptr;
        auto *obj = GetDisplayCameraObject();
        if (!obj) return nullptr;
        outTransform = obj->GetComponent<Transform>();
        return obj->GetComponent<Camera3D>();
    }

    /// @brief メッシュ（バインドポーズ）のローカル空間バウンディングボックスを取得する
    ///        （メッシュハンドルが変わらない限りキャッシュを使い回す）
    bool GetCachedLocalAabb(ModelManager::ModelHandle meshHandle, Vector3 &outMin, Vector3 &outMax) const {
        if (meshHandle == ModelManager::kInvalidHandle) return false;
        if (cachedAabbMeshHandle_ != meshHandle) {
            const auto &vertices = ModelManager::GetModelData(meshHandle).GetVertices();
            if (vertices.empty()) return false;
            Vector3 minV(vertices[0].px, vertices[0].py, vertices[0].pz);
            Vector3 maxV = minV;
            for (const auto &v : vertices) {
                minV.x = std::min(minV.x, v.px); maxV.x = std::max(maxV.x, v.px);
                minV.y = std::min(minV.y, v.py); maxV.y = std::max(maxV.y, v.py);
                minV.z = std::min(minV.z, v.pz); maxV.z = std::max(maxV.z, v.pz);
            }
            cachedAabbMin_ = minV;
            cachedAabbMax_ = maxV;
            cachedAabbMeshHandle_ = meshHandle;
        }
        outMin = cachedAabbMin_;
        outMax = cachedAabbMax_;
        return true;
    }

    /// @brief 線分（origin + dir*t, t∈[0,1]）とAABBの交差判定（スラブ法）
    static bool SegmentIntersectsAabb(const Vector3 &origin, const Vector3 &dir, const Vector3 &boxMin, const Vector3 &boxMax) {
        float tMin = 0.0f;
        float tMax = 1.0f;
        const float originArr[3] = { origin.x, origin.y, origin.z };
        const float dirArr[3] = { dir.x, dir.y, dir.z };
        const float minArr[3] = { boxMin.x, boxMin.y, boxMin.z };
        const float maxArr[3] = { boxMax.x, boxMax.y, boxMax.z };
        for (int i = 0; i < 3; ++i) {
            if (std::abs(dirArr[i]) < 1e-8f) {
                if (originArr[i] < minArr[i] || originArr[i] > maxArr[i]) return false;
                continue;
            }
            const float invD = 1.0f / dirArr[i];
            float t0 = (minArr[i] - originArr[i]) * invD;
            float t1 = (maxArr[i] - originArr[i]) * invD;
            if (t0 > t1) std::swap(t0, t1);
            tMin = std::max(tMin, t0);
            tMax = std::min(tMax, t1);
            if (tMin > tMax) return false;
        }
        return true;
    }

    /// @brief 線分（origin + dir*t, t∈[0,1]）と三角形の交差判定（Möller–Trumbore法、両面判定）
    /// @details SceneEditorView::SegmentIntersectsTriangleと同じアルゴリズム（実行時からも使えるよう
    ///          このコンポーネント側にも同じものを持たせている。エディター専用コードとは共有しない）
    static bool SegmentIntersectsTriangle(const Vector3 &origin, const Vector3 &dir,
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

    /// @brief メッシュの全三角形のいずれかと線分が交差するかどうか
    static bool IntersectsMeshTriangles(ModelManager::ModelHandle meshHandle, const Vector3 &origin, const Vector3 &dir) {
        const auto &model = ModelManager::GetModelData(meshHandle);
        const auto &vertices = model.GetVertices();
        const auto &indices = model.GetIndices();
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            const size_t indexA = static_cast<size_t>(indices[i]);
            const size_t indexB = static_cast<size_t>(indices[i + 1]);
            const size_t indexC = static_cast<size_t>(indices[i + 2]);
            // インポート失敗や編集中のアセット差し替えで壊れたインデックスが混ざっても範囲外参照しない
            if (indexA >= vertices.size() || indexB >= vertices.size() || indexC >= vertices.size()) continue;
            const auto &a = vertices[indexA];
            const auto &b = vertices[indexB];
            const auto &c = vertices[indexC];
            float t = 0.0f;
            if (SegmentIntersectsTriangle(origin, dir,
                    Vector3(a.px, a.py, a.pz), Vector3(b.px, b.py, b.pz), Vector3(c.px, c.py, c.pz), t)) {
                return true;
            }
        }
        return false;
    }

    /// @brief 対象（MeshRenderer/SkinnedMeshRenderer）の描画先ウィンドウ・表示カメラ(Camera3D)から、
    ///        現在のマウス座標に対応するワールド空間のレイ（近平面上の点と遠平面上の点）を計算する
    /// @details ScreenBufferViewport::TryGetOffscreenMousePositionと同じ変換パイプライン
    ///          （Windowクライアント座標 → NDC）の先を、Camera3Dの逆ビュー射影で近平面/遠平面へ
    ///          逆射影する形にしたもの。ComputeIsHovered/ComputeAxisDragOffsetの両方から使う
    bool ComputeMouseWorldRay(Vector3 &outRayStart, Vector3 &outRayEnd) const {
        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return false;

        auto *meshRenderer = objectContext->GetComponent<MeshRenderer>();
        auto *skinnedMeshRenderer = meshRenderer ? nullptr : objectContext->GetComponent<SkinnedMeshRenderer>();
        if (!meshRenderer && !skinnedMeshRenderer) return false;

        EmptyObject *targetObj = meshRenderer ? meshRenderer->GetTargetObject() : skinnedMeshRenderer->GetTargetObject();
        auto *windowComponent = ResolveWindow(targetObj);
        Window *window = windowComponent ? windowComponent->GetWindow() : nullptr;
        if (!window || !Window::IsExist(window)) return false;

        Transform *cameraTransform = nullptr;
        Camera3D *camera3d = ResolveDisplayCamera(cameraTransform);
        if (!camera3d || !cameraTransform) return false;

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

        // 2. NDC（近平面z=0/遠平面z=1）→ ワールド空間のレイ（表示カメラの逆ビュー射影行列）
        Matrix4x4 projection;
        projection.MakePerspectiveFovMatrix(camera3d->GetFovY(), camera3d->GetAspectRatio(), camera3d->GetNearClip(), camera3d->GetFarClip());
        const Matrix4x4 view = cameraTransform->GetWorldMatrix().Inverse();
        const Matrix4x4 viewProjectionInverse = (view * projection).Inverse();
        outRayStart = Vector3(ndcX, ndcY, 0.0f).Transform(viewProjectionInverse);
        outRayEnd = Vector3(ndcX, ndcY, 1.0f).Transform(viewProjectionInverse);
        return true;
    }

    /// @brief 現在のマウス座標がオブジェクトの当たり判定内にあるかを判定する
    /// @details ComputeMouseWorldRayでワールド空間のレイを作る → オブジェクトの逆ワールド行列でレイを
    ///          ローカル空間へ（アフィン変換なので線分パラメータtは保存される）変換し、メッシュの
    ///          ローカル空間バウンディングボックスと交差判定する。preciseMeshTest_が有効かつ
    ///          MeshRendererの場合のみ、バウンディングボックスに交差した後さらに全三角形との
    ///          交差判定まで行う
    bool ComputeIsHovered() const {
        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return false;

        auto *meshFilter = objectContext->GetComponent<MeshFilter>();
        if (!meshFilter || !meshFilter->HasMesh()) return false;

        auto *meshRenderer = objectContext->GetComponent<MeshRenderer>();

        Vector3 rayWorldStart, rayWorldEnd;
        if (!ComputeMouseWorldRay(rayWorldStart, rayWorldEnd)) return false;

        // レイをオブジェクトのローカル空間へ変換する（アフィン変換なので線分パラメータtは保存される）
        auto *transform = objectContext->GetComponent<Transform>();
        const Matrix4x4 world = transform ? transform->GetWorldMatrix() : Matrix4x4::Identity();
        const Matrix4x4 worldInverse = world.Inverse();
        const Vector3 localStart = rayWorldStart.Transform(worldInverse);
        const Vector3 localDir = rayWorldEnd.Transform(worldInverse) - localStart;

        // バウンディングボックス判定（常に実行。preciseMeshTest_無効時はこれが最終結果）
        const auto meshHandle = meshFilter->GetMeshHandle();
        Vector3 aabbMin, aabbMax;
        if (!GetCachedLocalAabb(meshHandle, aabbMin, aabbMax)) return false;
        if (!SegmentIntersectsAabb(localStart, localDir, aabbMin, aabbMax)) return false;

        // MeshRenderer限定・preciseMeshTest_有効時のみ、三角形との正確な交差判定まで行う
        if (meshRenderer && preciseMeshTest_) {
            return IntersectsMeshTriangles(meshHandle, localStart, localDir);
        }
        return true;
    }

    /// @brief 掴んで軸沿いにドラッグする操作の基準（軸原点・軸方向）を、現在のTransformから記録する
    void CaptureDragAxisReference() {
        hasDragAxisReference_ = false;
        auto *objectContext = GetOwnerObjectContext();
        auto *transform = objectContext ? objectContext->GetComponent<Transform>() : nullptr;
        if (!transform) return;

        const Matrix4x4 world = transform->GetWorldMatrix();
        dragAxisOriginWorld_ = Vector3(world.m[3][0], world.m[3][1], world.m[3][2]);

        // ローカル軸を平行移動成分を除いた回転・スケールだけで変換する（Vector3 * Matrix4x4は方向用）
        Vector3 dirWorld = dragAxis_ * world;
        if (dirWorld.Length() <= 1e-6f) return;
        dragAxisDirWorld_ = dirWorld.Normalize();
        hasDragAxisReference_ = true;
    }

    /// @brief 現在のマウスレイと、掴んだ瞬間に記録した軸（無限直線）との最近接点を求め、
    ///        軸原点からの符号付き距離（ワールド単位）を返す
    /// @details 2直線間の最近接点を求める標準的な式（Ericson "Real-Time Collision Detection"）。
    ///          カメラ視線とドラッグ軸がほぼ平行な場合は分母が0に近づき不安定になるため、
    ///          その場合はfalseを返す
    bool ComputeAxisDragOffset(const Vector3 &rayOrigin, const Vector3 &rayDir, float &outOffset) const {
        const Vector3 &d1 = rayDir;
        const Vector3 &d2 = dragAxisDirWorld_;
        const Vector3 r = rayOrigin - dragAxisOriginWorld_;

        const float a = d1.Dot(d1);
        const float e = d2.Dot(d2);
        const float b = d1.Dot(d2);
        const float c = d1.Dot(r);
        const float f = d2.Dot(r);

        constexpr float kEpsilon = 1e-6f;
        if (a <= kEpsilon || e <= kEpsilon) return false;
        const float denom = a * e - b * b;
        if (std::abs(denom) <= kEpsilon) return false;

        outOffset = (a * f - b * c) / denom;
        return true;
    }

    UUID128 displayCameraObjectID_{};
    bool preciseMeshTest_ = false;
    Vector3 dragAxis_{ 1.0f, 0.0f, 0.0f };
    bool isHovered_ = false;
    bool isPressed_ = false;
    bool isClicked_ = false;

    mutable ModelManager::ModelHandle cachedAabbMeshHandle_ = ModelManager::kInvalidHandle;
    mutable Vector3 cachedAabbMin_{};
    mutable Vector3 cachedAabbMax_{};

    bool hasDragAxisReference_ = false;
    Vector3 dragAxisOriginWorld_{};
    Vector3 dragAxisDirWorld_{};
    bool hasValidDragOffset_ = false;
    float lastDragOffset_ = 0.0f;
};

REGISTER_COMPONENT_OBJECT(MeshButton)

} // namespace KashipanEngine
