#pragma once
#include <cmath>
#include <functional>
#include <optional>

#include "Debug/Logger.h"
#include "Objects/ObjectComponentHeader.h"
#include "Objects/Collision/Collider.h"
#include "Math/Quaternion.h"
#include "Math/Vector2.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief 当たり判定用コンポーネントの基底クラス
/// @details 派生クラスは全て Collision カテゴリに分類される。
///          生成時（Initialize）にシーンの SceneObjectCollider コンポーネントへポインタ登録され、
///          以後 SceneObjectCollider が毎フレーム BuildColliderInfo2D/3D を呼んで最新状態を
///          読み取るため、派生クラス側で値が変わっても再登録は不要。
class ICollider : public IObjectComponent {
public:
    enum class Shape {
        Box,
        Sphere,
        Capsule,
        Ray,
        Mesh,
        Ray2D,
        Box2D,
        Circle2D,
        Capsule2D,
    };

    // 派生クラスは全て Collision カテゴリに分類される
    COMPONENT_CATEGORY("Collision")

    Shape GetShape() const noexcept { return shape_; }
    /// @brief 2D用コライダーかどうか（falseの場合は3D用）
    bool Is2D() const noexcept { return is2D_; }
    bool IsTrigger() const noexcept { return isTrigger_; }
    void SetTrigger(bool isTrigger) noexcept { isTrigger_ = isTrigger; }

    /// @brief 連続衝突判定（CCD）が有効かどうか
    bool IsContinuousDetection() const noexcept { return continuousDetection_; }
    /// @brief 連続衝突判定（CCD）の有効/無効を設定する
    /// @details 有効にすると、前フレーム位置から現在位置までの移動経路を形状サイズ以下の刻みに分割して
    ///          中間位置でも判定するため、高速移動で他のコライダーをすり抜けた場合でも
    ///          衝突イベント（OnCollisionEnter等）が発生する。高速に動く弾丸や小さいオブジェクトに使用する
    void SetContinuousDetection(bool enabled) noexcept { continuousDetection_ = enabled; }

    /// @brief 他のICollider（通常は複製元）からTransform同期設定・判定設定をコピーする（Cloneで使用）
    void CopySyncSettingsFrom(const ICollider &other) {
        LogScope scope;
        for (int i = 0; i < 3; ++i) {
            syncPosition_[i] = other.syncPosition_[i];
            syncScale_[i] = other.syncScale_[i];
        }
        syncRotation_ = other.syncRotation_;
        continuousDetection_ = other.continuousDetection_;
    }

    //==================================================
    // 衝突コールバック（3D用）
    //==================================================

    void SetOnCollisionEnter3D(std::function<void(const HitInfo3D &)> callback) { onCollisionEnter3D_ = std::move(callback); }
    void SetOnCollisionStay3D(std::function<void(const HitInfo3D &)> callback) { onCollisionStay3D_ = std::move(callback); }
    void SetOnCollisionExit3D(std::function<void(const HitInfo3D &)> callback) { onCollisionExit3D_ = std::move(callback); }
    const std::function<void(const HitInfo3D &)> &GetOnCollisionEnter3D() const noexcept { return onCollisionEnter3D_; }
    const std::function<void(const HitInfo3D &)> &GetOnCollisionStay3D() const noexcept { return onCollisionStay3D_; }
    const std::function<void(const HitInfo3D &)> &GetOnCollisionExit3D() const noexcept { return onCollisionExit3D_; }

    //==================================================
    // 衝突コールバック（2D用）
    //==================================================

    void SetOnCollisionEnter2D(std::function<void(const HitInfo2D &)> callback) { onCollisionEnter2D_ = std::move(callback); }
    void SetOnCollisionStay2D(std::function<void(const HitInfo2D &)> callback) { onCollisionStay2D_ = std::move(callback); }
    void SetOnCollisionExit2D(std::function<void(const HitInfo2D &)> callback) { onCollisionExit2D_ = std::move(callback); }
    const std::function<void(const HitInfo2D &)> &GetOnCollisionEnter2D() const noexcept { return onCollisionEnter2D_; }
    const std::function<void(const HitInfo2D &)> &GetOnCollisionStay2D() const noexcept { return onCollisionStay2D_; }
    const std::function<void(const HitInfo2D &)> &GetOnCollisionExit2D() const noexcept { return onCollisionExit2D_; }

    //==================================================
    // 形状情報の構築（SceneObjectCollider から毎フレーム呼ばれる）
    //==================================================

    /// @brief 3D用の形状情報を構築する（3D系コライダーで実装。常駐形状を持たない場合はnullopt）
    virtual std::optional<ColliderInfo3D> BuildColliderInfo3D() const { return std::nullopt; }
    /// @brief 2D用の形状情報を構築する（2D系コライダーで実装。常駐形状を持たない場合はnullopt）
    virtual std::optional<ColliderInfo2D> BuildColliderInfo2D() const { return std::nullopt; }

    /// @brief オーナーオブジェクトのワールド座標を取得（Transformが無い場合は原点）
    /// @details デバッグシーンビューでの当たり判定可視化にも使用するため公開している
    Vector3 GetOwnerWorldPosition() const;

    //==================================================
    // Transform同期設定
    //==================================================
    // 位置・スケールはXYZ軸ごとに、同オブジェクトのTransformへ追従させるかを個別に選択できる。
    // 無効にした軸は各コライダーの形状計算において「値なし」（位置は0、スケールは1）として扱われる。
    // 回転は軸ごとの部分同期を行うと、オイラー角分解の順序依存とジンバルロックにより
    // 反対向きの回転や不連続なジャンプが発生してしまうため、「全軸同期」か「非同期（Identity）」の
    // 二択のみとしている（部分同期はサポートしない）。

    bool IsSyncPositionEnabled(int axis) const noexcept { return syncPosition_[axis]; }
    bool IsSyncRotationEnabled() const noexcept { return syncRotation_; }
    bool IsSyncScaleEnabled(int axis) const noexcept { return syncScale_[axis]; }

    /// @brief 同期設定を考慮したオーナーのワールド座標を取得する（無効な軸は0として扱う）
    Vector3 GetSyncedOwnerPosition() const;
    /// @brief 同期設定を考慮したオーナーの回転（オイラー角、ラジアン、ローカル）を取得する（無効な軸は0として扱う）
    Vector3 GetSyncedOwnerRotationEuler() const;
    /// @brief 同期設定を考慮したオーナーの回転（クォータニオン）を取得する
    Quaternion GetSyncedOwnerRotation() const;
    /// @brief 同期設定を考慮したオーナーのスケール（ローカル。Transformにワールドスケール取得APIが無いため）を取得する（無効な軸は1として扱う）
    Vector3 GetSyncedOwnerScale() const;

    /// @brief 同期設定のZ回転を考慮して、2D用のローカルオフセットを回転させる
    Vector2 RotateOffsetBySyncedRotation2D(const Vector2 &localOffset) const {
        LogScope scope;
        const float angle = GetSyncedOwnerRotationEuler().z;
        if (angle == 0.0f) return localOffset;
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        return Vector2(localOffset.x * c - localOffset.y * s, localOffset.x * s + localOffset.y * c);
    }

protected:
    ICollider(const std::string &typeName, Shape shape, bool is2D, size_t componentTypeID)
        : IObjectComponent(typeName, 0xFF, componentTypeID), shape_(shape), is2D_(is2D) {
        LogScope scope;
        // 共通のImGui編集パラメータを外部アクセス用に登録する（形状パラメータは各派生クラスで登録）
        ADD_MEMBER_VARIABLE(isTrigger_);
        ADD_MEMBER_VARIABLE(continuousDetection_);
    }

    /// @brief SceneObjectColliderへ自身を登録する（定義はEmptyObject/SceneObjectColliderの完全な型が必要なためICollider.cppにある）
    void Initialize() override;
    /// @brief SceneObjectColliderから自身の登録を解除する（定義はICollider.cppにある）
    void Finalize() override;

#if defined(USE_IMGUI)
    void ShowImGui() override {
        LogScope scope;
        ImGui::Checkbox(TranslationLabel("component.icollider.istrigger"), &isTrigger_);
        ImGui::Checkbox(TranslationLabel("component.icollider.continuous_detection"), &continuousDetection_);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", "高速移動時のすり抜けでも衝突イベントを発生させる（連続衝突判定）");
        }
        ImGui::Separator();
        ImGui::TextUnformatted(TranslationC("component.icollider.sync_with_transform"));
        if (is2D_) {
            ImGui::Checkbox(TranslationLabel("component.icollider.pos_x"), &syncPosition_[0]); ImGui::SameLine();
            ImGui::Checkbox(TranslationLabel("component.icollider.pos_y"), &syncPosition_[1]);
            ImGui::Checkbox(TranslationLabel("component.icollider.rotation"), &syncRotation_);
            ImGui::Checkbox(TranslationLabel("component.icollider.scale_x"), &syncScale_[0]); ImGui::SameLine();
            ImGui::Checkbox(TranslationLabel("component.icollider.scale_y"), &syncScale_[1]);
        } else {
            ImGui::Checkbox(TranslationLabel("component.icollider.pos_x"), &syncPosition_[0]); ImGui::SameLine();
            ImGui::Checkbox(TranslationLabel("component.icollider.pos_y"), &syncPosition_[1]); ImGui::SameLine();
            ImGui::Checkbox(TranslationLabel("component.icollider.pos_z"), &syncPosition_[2]);
            ImGui::Checkbox(TranslationLabel("component.icollider.rotation"), &syncRotation_);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", "回転は軸ごとの部分同期をサポートしない（数学的に反転・不連続が避けられないため）");
            }
            ImGui::Checkbox(TranslationLabel("component.icollider.scale_x"), &syncScale_[0]); ImGui::SameLine();
            ImGui::Checkbox(TranslationLabel("component.icollider.scale_y"), &syncScale_[1]); ImGui::SameLine();
            ImGui::Checkbox(TranslationLabel("component.icollider.scale_z"), &syncScale_[2]);
        }
    }
#endif

    JSON SaveToJson() const override {
        LogScope scope;
        JSON json = JSON::object();
        json["isTrigger"] = isTrigger_;
        json["continuousDetection"] = continuousDetection_;
        json["syncPosition"] = { syncPosition_[0], syncPosition_[1], syncPosition_[2] };
        json["syncRotation"] = syncRotation_;
        json["syncScale"] = { syncScale_[0], syncScale_[1], syncScale_[2] };
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        LogScope scope;
        isTrigger_ = json.value("isTrigger", false);
        continuousDetection_ = json.value("continuousDetection", false);
        if (json.contains("syncPosition") && json["syncPosition"].is_array() && json["syncPosition"].size() == 3) {
            for (int i = 0; i < 3; ++i) syncPosition_[i] = json["syncPosition"][i].get<bool>();
        }
        if (json.contains("syncRotation")) {
            const auto &syncRotationJson = json["syncRotation"];
            if (syncRotationJson.is_array() && syncRotationJson.size() == 3) {
                // 旧形式（軸ごとの部分同期）からの読み込み: 全軸有効だった場合のみ同期扱いとする
                syncRotation_ = syncRotationJson[0].get<bool>() && syncRotationJson[1].get<bool>() && syncRotationJson[2].get<bool>();
            } else if (syncRotationJson.is_boolean()) {
                syncRotation_ = syncRotationJson.get<bool>();
            }
        }
        if (json.contains("syncScale") && json["syncScale"].is_array() && json["syncScale"].size() == 3) {
            for (int i = 0; i < 3; ++i) syncScale_[i] = json["syncScale"][i].get<bool>();
        }
        return true;
    }

private:
    Shape shape_;
    bool is2D_ = false;
    bool isTrigger_ = false;
    /// @brief 連続衝突判定（CCD）の有効/無効
    bool continuousDetection_ = false;

    // Transform同期設定（位置・回転はデフォルトで追従、スケールは既存挙動を変えないためデフォルト非追従）
    bool syncPosition_[3] = { true, true, true };
    /// @brief 回転の同期。軸ごとの部分同期はサポートせず、全軸同期(true)か非同期(false)の二択
    bool syncRotation_ = true;
    bool syncScale_[3] = { false, false, false };

    std::function<void(const HitInfo3D &)> onCollisionEnter3D_;
    std::function<void(const HitInfo3D &)> onCollisionStay3D_;
    std::function<void(const HitInfo3D &)> onCollisionExit3D_;

    std::function<void(const HitInfo2D &)> onCollisionEnter2D_;
    std::function<void(const HitInfo2D &)> onCollisionStay2D_;
    std::function<void(const HitInfo2D &)> onCollisionExit2D_;
};

} // namespace KashipanEngine
