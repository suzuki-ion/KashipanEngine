#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>

#include "Objects/ObjectComponentHeader.h"
#include "Assets/ModelManager.h"
#include "Math/Matrix4x4.h"
#include "Math/Quaternion.h"
#include "Math/Vector3.h"
#include "Objects/Components/MeshFilter.h"
#include "Objects/Components/Render/SpriteRenderer.h"
#include "Objects/Components/Render/TextRenderer.h"
#include "Objects/Components/Transform.h"
#include "Utilities/UUID128.h"
#if defined(USE_IMGUI)
#include "Objects/Components/Render/TargetObjectSelector.h"
#include "Utilities/Translation.h"
#endif

namespace KashipanEngine {

/// @brief 参照先オブジェクトの「範囲」の中の基準点へ、自身の位置を毎フレーム追従させるコンポーネント
/// @details ScreenAnchorの基準を「Camera2Dの画面」から「任意のオブジェクトの範囲」に置き換えたもの。
///          範囲は参照先の持つ描画コンポーネントから求め、2D（スプライト・テキスト）と3D（メッシュ）の
///          どちらにも同じ設定で使える。
///          - Mesh: MeshFilterのメッシュの外接箱（ローカル空間）。SpriteRendererを持つ場合はアンカー・ピボット
///            補正込みのワールド行列（SpriteRenderer::GetWorldMatrix）を使うため、見た目の矩形と一致する
///          - Text: TextRendererの全文字の外接矩形（参照先Transformのローカル空間）
///          - Transform: 参照先Transformの原点を中心とした1×1×1の箱（描画コンポーネントを持たない場合）
///          基準点(anchorPoint)は範囲の最小側の角を(0,0,0)、最大側の角を(1,1,1)とした割合で指定する
///          （2DではX=左→右、Y=下→上。範囲は参照先のローカル空間で求めるため、参照先が回転・拡縮しても
///          範囲上の同じ点に追従する）。TransformSyncと同様、シーン階層上の親子関係は変更しない。
class ObjectAnchor final : public IObjectComponent {
public:
    /// @brief 参照先の範囲をどの情報から求めるか
    enum class BoundsSource {
        Auto,      ///< Mesh（スプライト含む）→Text→Transformの順で、参照先が持つものを使う
        Mesh,      ///< MeshFilterのメッシュ（SpriteRendererを持つ場合はスプライトの矩形）
        Text,      ///< TextRendererの文字全体の外接矩形
        Transform, ///< Transformを中心とした1×1×1の箱（スケールが大きさになる）
    };

    OBJECT_COMPONENT_CONSTRUCTOR(ObjectAnchor, 0xFF,
        ADD_MEMBER_VARIABLE(anchorPoint_);
        ADD_MEMBER_VARIABLE(offset_);
        ADD_MEMBER_VARIABLE(offsetInTargetSpace_);
        ADD_MEMBER_VARIABLE(applyX_);
        ADD_MEMBER_VARIABLE(applyY_);
        ADD_MEMBER_VARIABLE(applyZ_);
        ADD_MEMBER_VARIABLE(followRotation_);
    )
    COMPONENT_CATEGORY("Utility")
    ~ObjectAnchor() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<ObjectAnchor>();
        ptr->targetObjectID_ = targetObjectID_;
        ptr->boundsSource_ = boundsSource_;
        ptr->anchorPoint_ = anchorPoint_;
        ptr->offset_ = offset_;
        ptr->offsetInTargetSpace_ = offsetInTargetSpace_;
        ptr->applyX_ = applyX_;
        ptr->applyY_ = applyY_;
        ptr->applyZ_ = applyZ_;
        ptr->followRotation_ = followRotation_;
        return ptr;
    }

    //==================================================
    // 参照先
    //==================================================

    /// @brief 参照先オブジェクトを設定する
    void SetTargetObject(const EmptyObject *targetObject) {
        targetObjectID_ = targetObject ? targetObject->GetObjectID() : UUID128();
    }
    /// @brief 参照先オブジェクトをUUIDから設定する
    void SetTargetObject(const UUID128 &targetObjectID) { targetObjectID_ = targetObjectID; }
    const UUID128 &GetTargetObjectID() const noexcept { return targetObjectID_; }
    /// @brief 参照先オブジェクトを取得する（存在しない場合は nullptr）
    EmptyObject *GetTargetObject() const {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext || !targetObjectID_.IsValid()) return nullptr;
        return sceneContext->GetSceneObject(targetObjectID_);
    }

    //==================================================
    // 基準点・オフセット
    //==================================================

    /// @brief 範囲の求め方を設定する
    void SetBoundsSource(BoundsSource source) noexcept { boundsSource_ = source; }
    BoundsSource GetBoundsSource() const noexcept { return boundsSource_; }

    /// @brief 範囲内の基準点を設定する（(0,0,0)=範囲の最小側の角 ～ (1,1,1)=最大側の角の割合。範囲外の値も可）
    void SetAnchorPoint(const Vector3 &anchorPoint) noexcept { anchorPoint_ = anchorPoint; }
    const Vector3 &GetAnchorPoint() const noexcept { return anchorPoint_; }

    /// @brief 基準点からのオフセット（ワールド単位）を設定する
    void SetOffset(const Vector3 &offset) noexcept { offset_ = offset; }
    const Vector3 &GetOffset() const noexcept { return offset_; }

    /// @brief trueの場合、オフセットを参照先のワールド回転で回してから加える（falseはワールド軸のまま加える）
    void SetOffsetInTargetSpace(bool enable) noexcept { offsetInTargetSpace_ = enable; }
    bool GetOffsetInTargetSpace() const noexcept { return offsetInTargetSpace_; }

    /// @brief 追従結果を反映する軸を設定する（自身の親から見たローカル座標の軸単位。外した軸は現在値を保つ）
    void SetApplyAxes(bool x, bool y, bool z) noexcept { applyX_ = x; applyY_ = y; applyZ_ = z; }
    void SetApplyX(bool enable) noexcept { applyX_ = enable; }
    void SetApplyY(bool enable) noexcept { applyY_ = enable; }
    void SetApplyZ(bool enable) noexcept { applyZ_ = enable; }
    bool GetApplyX() const noexcept { return applyX_; }
    bool GetApplyY() const noexcept { return applyY_; }
    bool GetApplyZ() const noexcept { return applyZ_; }

    /// @brief trueの場合、位置に加えて回転も参照先のワールド回転へ追従させる
    void SetFollowRotation(bool enable) noexcept { followRotation_ = enable; }
    bool GetFollowRotation() const noexcept { return followRotation_; }

    //==================================================
    // 計算結果
    //==================================================

    /// @brief 現在の設定での追従先（基準点＋オフセット）のワールド座標を求める
    /// @return 参照先が無い、または自身・自身の子孫を参照している場合は false
    bool TryGetAnchorWorldPosition(Vector3 &outPosition) const {
        EmptyObject *target = GetValidTarget();
        if (!target) return false;
        AnchorBounds bounds;
        if (!ComputeBounds(target, bounds)) return false;
        outPosition = ComputeAnchorWorldPosition(target, bounds);
        return true;
    }

    /// @brief 直近の追従で実際に使われた範囲の種類（Auto指定時は解決後の種類）
    BoundsSource GetResolvedBoundsSource() const noexcept { return resolvedBoundsSource_; }

protected:
    void Update() override {
        ApplyAnchor();
    }

#if defined(USE_IMGUI)
    /// @brief エディターでPlayしていない間も追従させるためのフック
    /// @details Update()はPlay中にしか呼ばれないため、ScreenAnchorと同様にこちらでも追従処理を行う
    void ShowPersistentImGui() override {
        ApplyAnchor();
    }

    void ShowImGui() override {
        TargetObjectSelector::ShowSelector(TranslationLabel("component.objectanchor.target"), GetOwnerSceneContext(), targetObjectID_, true, false);
        if (!targetObjectID_.IsValid()) {
            ImGuiCustom::TextDisabledWrapped("%s", TranslationC("component.objectanchor.no_target"));
        } else if (GetTargetObject() && !GetValidTarget()) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "%s", TranslationC("component.objectanchor.invalid_target"));
        }

        //--------- 範囲の求め方 ---------//
        const char *sourceItems[] = {
            TranslationC("component.objectanchor.bounds_source.auto"),
            TranslationC("component.objectanchor.bounds_source.mesh"),
            TranslationC("component.objectanchor.bounds_source.text"),
            TranslationC("component.objectanchor.bounds_source.transform"),
        };
        int source = static_cast<int>(boundsSource_);
        if (ImGui::Combo(TranslationLabel("component.objectanchor.bounds_source"), &source, sourceItems, IM_ARRAYSIZE(sourceItems))) {
            boundsSource_ = static_cast<BoundsSource>(source);
        }
        if (GetValidTarget()) {
            // Auto指定時や、指定した情報を参照先が持たずTransformへフォールバックした場合に分かるよう表示する
            ImGuiCustom::TextDisabledWrapped("%s%s", TranslationC("component.objectanchor.resolved"),
                sourceItems[static_cast<int>(resolvedBoundsSource_)]);
        }

        ImGui::Separator();

        //--------- 基準点 ---------//
        ImGui::DragFloat3(TranslationLabel("component.objectanchor.anchor_point"), &anchorPoint_.x, 0.01f);
        if (ImGui::IsItemHovered()) {
            ImGuiCustom::SetTooltipWrapped("%s", TranslationC("component.objectanchor.anchor_point.tooltip"));
        }
        ShowAnchorPresetGrid();

        //--------- オフセット ---------//
        ImGui::DragFloat3(TranslationLabel("component.objectanchor.offset"), &offset_.x, 0.01f);
        ImGui::Checkbox(TranslationLabel("component.objectanchor.offset_in_target_space"), &offsetInTargetSpace_);

        ImGui::Separator();

        //--------- 反映する軸・回転 ---------//
        ImGui::TextUnformatted(TranslationC("component.objectanchor.apply_axes"));
        ImGui::SameLine();
        ImGui::Checkbox("X##applyX", &applyX_);
        ImGui::SameLine();
        ImGui::Checkbox("Y##applyY", &applyY_);
        ImGui::SameLine();
        ImGui::Checkbox("Z##applyZ", &applyZ_);
        ImGui::Checkbox(TranslationLabel("component.objectanchor.follow_rotation"), &followRotation_);

        ImGuiCustom::TextDisabledWrapped("%s", TranslationC("component.objectanchor.desc"));
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["targetObjectID"] = ToJSON(targetObjectID_);
        json["boundsSource"] = static_cast<int>(boundsSource_);
        json["anchorPoint"] = ToJSON(anchorPoint_);
        json["offset"] = ToJSON(offset_);
        json["offsetInTargetSpace"] = offsetInTargetSpace_;
        json["applyX"] = applyX_;
        json["applyY"] = applyY_;
        json["applyZ"] = applyZ_;
        json["followRotation"] = followRotation_;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        targetObjectID_ = json.contains("targetObjectID") ? FromJSON<UUID128>(json["targetObjectID"]) : UUID128();
        const int source = json.value("boundsSource", static_cast<int>(BoundsSource::Auto));
        boundsSource_ = (source >= 0 && source <= static_cast<int>(BoundsSource::Transform))
            ? static_cast<BoundsSource>(source) : BoundsSource::Auto;
        anchorPoint_ = json.contains("anchorPoint") ? FromJSON<Vector3>(json["anchorPoint"]) : Vector3(0.5f, 0.5f, 0.5f);
        offset_ = json.contains("offset") ? FromJSON<Vector3>(json["offset"]) : Vector3(0.0f, 0.0f, 0.0f);
        offsetInTargetSpace_ = json.value("offsetInTargetSpace", false);
        applyX_ = json.value("applyX", true);
        applyY_ = json.value("applyY", true);
        applyZ_ = json.value("applyZ", true);
        followRotation_ = json.value("followRotation", false);
        return true;
    }

private:
    /// @brief 参照先の範囲（frameのローカル空間でのAABB）
    struct AnchorBounds {
        Matrix4x4 frame = Matrix4x4::Identity();
        Vector3 localMin{ -0.5f, -0.5f, -0.5f };
        Vector3 localMax{ 0.5f, 0.5f, 0.5f };
        BoundsSource source = BoundsSource::Transform;
    };

    /// @brief 追従に使える参照先を返す（自身、または自身の子孫を参照している場合は nullptr）
    /// @details 子孫を参照すると「自身を動かす→子孫も動く→追従先が動く」のフィードバックで位置が発散するため除外する
    EmptyObject *GetValidTarget() const {
        EmptyObject *target = GetTargetObject();
        auto *objectContext = GetOwnerObjectContext();
        const EmptyObject *owner = objectContext ? objectContext->GetOwner() : nullptr;
        if (!target || !owner) return nullptr;
        for (EmptyObject *cursor = target; cursor;) {
            if (cursor == owner) return nullptr;
            auto *transform = cursor->GetComponent<Transform>();
            cursor = transform ? transform->GetParentObject() : nullptr;
        }
        return target;
    }

    /// @brief 参照先の範囲を求める（指定した情報を参照先が持たない場合はTransformの箱へフォールバックする）
    bool ComputeBounds(EmptyObject *target, AnchorBounds &out) const {
        auto *targetTransform = target->GetComponent<Transform>();
        if (!targetTransform) return false;

        const bool tryMesh = boundsSource_ == BoundsSource::Auto || boundsSource_ == BoundsSource::Mesh;
        const bool tryText = boundsSource_ == BoundsSource::Auto || boundsSource_ == BoundsSource::Text;
        if (tryMesh && ComputeMeshBounds(target, out)) return true;
        if (tryText && ComputeTextBounds(target, targetTransform, out)) return true;

        out.frame = targetTransform->GetWorldMatrix();
        out.localMin = Vector3(-0.5f, -0.5f, -0.5f);
        out.localMax = Vector3(0.5f, 0.5f, 0.5f);
        out.source = BoundsSource::Transform;
        return true;
    }

    /// @brief MeshFilterのメッシュ（SpriteRendererを持つ場合はスプライトの矩形）から範囲を求める
    bool ComputeMeshBounds(EmptyObject *target, AnchorBounds &out) const {
        auto *meshFilter = target->GetComponent<MeshFilter>();
        auto *spriteRenderer = target->GetComponent<SpriteRenderer>();
        const bool hasMesh = meshFilter && meshFilter->HasMesh();
        if (!hasMesh && !spriteRenderer) return false;

        if (spriteRenderer) {
            // スプライトはアンカー・ピボット補正込みの行列で単位クアッド（-0.5～0.5）を描くため、その行列を使う
            out.frame = spriteRenderer->GetWorldMatrix();
        } else {
            auto *targetTransform = target->GetComponent<Transform>();
            if (!targetTransform) return false;
            out.frame = targetTransform->GetWorldMatrix();
        }

        if (hasMesh && GetMeshLocalBounds(meshFilter->GetMeshHandle(), out.localMin, out.localMax)) {
            out.source = BoundsSource::Mesh;
            return true;
        }
        if (!spriteRenderer) return false;
        // メッシュ未設定のスプライトは既定の単位クアッドとみなす
        out.localMin = Vector3(-0.5f, -0.5f, 0.0f);
        out.localMax = Vector3(0.5f, 0.5f, 0.0f);
        out.source = BoundsSource::Mesh;
        return true;
    }

    /// @brief メッシュのローカル外接箱を求める（頂点走査は重いため、メッシュとその内容の版数が変わった時だけ再計算する）
    bool GetMeshLocalBounds(ModelManager::ModelHandle handle, Vector3 &outMin, Vector3 &outMax) const {
        const std::uint64_t version = ModelManager::GetModelDataVersion(handle);
        if (handle != cachedMeshHandle_ || version != cachedMeshVersion_) {
            cachedMeshHandle_ = handle;
            cachedMeshVersion_ = version;
            const auto &vertices = ModelManager::GetModelData(handle).GetVertices();
            cachedMeshValid_ = !vertices.empty();
            if (cachedMeshValid_) {
                Vector3 minPos(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
                Vector3 maxPos(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());
                for (const auto &v : vertices) {
                    minPos = Vector3(std::min(minPos.x, v.px), std::min(minPos.y, v.py), std::min(minPos.z, v.pz));
                    maxPos = Vector3(std::max(maxPos.x, v.px), std::max(maxPos.y, v.py), std::max(maxPos.z, v.pz));
                }
                cachedMeshMin_ = minPos;
                cachedMeshMax_ = maxPos;
            }
        }
        if (!cachedMeshValid_) return false;
        outMin = cachedMeshMin_;
        outMax = cachedMeshMax_;
        return true;
    }

    /// @brief TextRendererの全文字の外接矩形を、参照先Transformのローカル空間で求める
    /// @details 各文字は単位クアッド（-0.5～0.5）を文字ごとのワールド行列で描いている
    ///          （SceneEditorViewのComputeTextRendererWorldBoundsと同じ前提）。参照先のローカル空間で
    ///          求めることで、テキストが回転していても文字列の矩形上の同じ点に追従できる
    bool ComputeTextBounds(EmptyObject *target, Transform *targetTransform, AnchorBounds &out) const {
        auto *textRenderer = target->GetComponent<TextRenderer>();
        if (!textRenderer) return false;
        const auto instances = textRenderer->GetRenderInstances();
        if (instances.empty()) return false;

        out.frame = targetTransform->GetWorldMatrix();
        const Matrix4x4 worldToLocal = out.frame.Inverse();
        const Vector3 corners[4] = {
            Vector3(-0.5f, -0.5f, 0.0f), Vector3(0.5f, -0.5f, 0.0f),
            Vector3(-0.5f, 0.5f, 0.0f), Vector3(0.5f, 0.5f, 0.0f),
        };
        Vector3 minPos(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
        Vector3 maxPos(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());
        for (const auto &instance : instances) {
            const Matrix4x4 instanceToLocal = instance.worldMatrix * worldToLocal;
            for (const auto &corner : corners) {
                const Vector3 p = corner.Transform(instanceToLocal);
                minPos = Vector3(std::min(minPos.x, p.x), std::min(minPos.y, p.y), std::min(minPos.z, p.z));
                maxPos = Vector3(std::max(maxPos.x, p.x), std::max(maxPos.y, p.y), std::max(maxPos.z, p.z));
            }
        }
        out.localMin = minPos;
        out.localMax = maxPos;
        out.source = BoundsSource::Text;
        return true;
    }

    /// @brief 範囲上の基準点にオフセットを加えたワールド座標を求める
    Vector3 ComputeAnchorWorldPosition(EmptyObject *target, const AnchorBounds &bounds) const {
        const Vector3 localPoint(
            bounds.localMin.x + (bounds.localMax.x - bounds.localMin.x) * anchorPoint_.x,
            bounds.localMin.y + (bounds.localMax.y - bounds.localMin.y) * anchorPoint_.y,
            bounds.localMin.z + (bounds.localMax.z - bounds.localMin.z) * anchorPoint_.z);
        Vector3 worldPoint = localPoint.Transform(bounds.frame);

        if (offsetInTargetSpace_) {
            auto *targetTransform = target->GetComponent<Transform>();
            worldPoint += targetTransform ? targetTransform->GetWorldRotateQuaternion().RotateVector(offset_) : offset_;
        } else {
            worldPoint += offset_;
        }
        return worldPoint;
    }

    /// @brief 参照先の基準点へ自身のTransformを追従させる（Update()/ShowPersistentImGui()共通処理）
    void ApplyAnchor() {
        auto *objectContext = GetOwnerObjectContext();
        auto *transform = objectContext ? objectContext->GetComponent<Transform>() : nullptr;
        if (!transform) return;
        EmptyObject *target = GetValidTarget();
        if (!target) return;
        AnchorBounds bounds;
        if (!ComputeBounds(target, bounds)) return;
        resolvedBoundsSource_ = bounds.source;

        auto *parentObject = transform->GetParentObject();
        auto *parentTransform = parentObject ? parentObject->GetComponent<Transform>() : nullptr;

        if (applyX_ || applyY_ || applyZ_) {
            const Vector3 desiredWorld = ComputeAnchorWorldPosition(target, bounds);
            // 自身に親がいる場合は親のローカル空間へ変換してから、有効な軸だけを書き換える
            const Vector3 desiredLocal = parentTransform
                ? desiredWorld.Transform(parentTransform->GetWorldMatrix().Inverse())
                : desiredWorld;
            Vector3 translate = transform->GetTranslate();
            if (applyX_) translate.x = desiredLocal.x;
            if (applyY_) translate.y = desiredLocal.y;
            if (applyZ_) translate.z = desiredLocal.z;
            // 値が変わらない場合はワールド行列の再計算（ダーティ化）を避ける
            if (translate != transform->GetTranslate()) transform->SetTranslate(translate);
        }

        if (followRotation_) {
            auto *targetTransform = target->GetComponent<Transform>();
            if (targetTransform) {
                const Quaternion desiredWorldRot = targetTransform->GetWorldRotateQuaternion();
                transform->SetRotateQuaternion(parentTransform
                    ? (desiredWorldRot * parentTransform->GetWorldRotateQuaternion().Inverse()).Normalize()
                    : desiredWorldRot);
            }
        }
    }

#if defined(USE_IMGUI)
    /// @brief 基準点のX/Yを9方向（左上～右下）から選ぶプリセットボタン（Zは変更しない）
    void ShowAnchorPresetGrid() {
        ImGui::TextUnformatted(TranslationC("component.objectanchor.anchor_preset"));
        ImGui::SameLine();
        ImGui::BeginGroup();
        constexpr float kButtonSize = 16.0f;
        constexpr float kRows[3] = { 1.0f, 0.5f, 0.0f }; // 上段（Y=1）から並べる
        constexpr float kColumns[3] = { 0.0f, 0.5f, 1.0f };
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                if (column > 0) ImGui::SameLine(0.0f, 2.0f);
                const float x = kColumns[column];
                const float y = kRows[row];
                const bool selected = std::abs(anchorPoint_.x - x) < 1e-4f && std::abs(anchorPoint_.y - y) < 1e-4f;
                ImGui::PushID(row * 3 + column);
                if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                if (ImGui::Button("##anchorPreset", ImVec2(kButtonSize, kButtonSize))) {
                    anchorPoint_.x = x;
                    anchorPoint_.y = y;
                }
                if (selected) ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("(%.1f, %.1f)", x, y);
                ImGui::PopID();
            }
        }
        ImGui::EndGroup();
    }
#endif

    UUID128 targetObjectID_{};
    BoundsSource boundsSource_ = BoundsSource::Auto;
    /// @brief 範囲内の基準点（(0,0,0)=最小側の角 ～ (1,1,1)=最大側の角）
    Vector3 anchorPoint_{ 0.5f, 0.5f, 0.5f };
    /// @brief 基準点からのオフセット（ワールド単位）
    Vector3 offset_{ 0.0f, 0.0f, 0.0f };
    bool offsetInTargetSpace_ = false;
    bool applyX_ = true;
    bool applyY_ = true;
    bool applyZ_ = true;
    bool followRotation_ = false;

    /// @brief 直近の追従で実際に使われた範囲の種類（インスペクター表示用）
    BoundsSource resolvedBoundsSource_ = BoundsSource::Transform;

    // メッシュのローカル外接箱のキャッシュ（GetMeshLocalBounds参照）
    mutable ModelManager::ModelHandle cachedMeshHandle_ = ModelManager::kInvalidHandle;
    mutable std::uint64_t cachedMeshVersion_ = 0;
    mutable bool cachedMeshValid_ = false;
    mutable Vector3 cachedMeshMin_{};
    mutable Vector3 cachedMeshMax_{};
};

REGISTER_COMPONENT_OBJECT(ObjectAnchor)

} // namespace KashipanEngine
