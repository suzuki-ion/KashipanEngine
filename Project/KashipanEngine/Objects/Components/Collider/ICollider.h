#pragma once
#include <functional>
#include <optional>

#include "Objects/ObjectComponentHeader.h"
#include "Objects/Collision/Collider.h"

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

protected:
    ICollider(const std::string &typeName, Shape shape, bool is2D, size_t componentTypeID)
        : IObjectComponent(typeName, 0xFF, componentTypeID), shape_(shape), is2D_(is2D) {}

    /// @brief SceneObjectColliderへ自身を登録する（定義はEmptyObject/SceneObjectColliderの完全な型が必要なためICollider.cppにある）
    void Initialize() override;
    /// @brief SceneObjectColliderから自身の登録を解除する（定義はICollider.cppにある）
    void Finalize() override;

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::Checkbox("IsTrigger", &isTrigger_);
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["isTrigger"] = isTrigger_;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        isTrigger_ = json.value("isTrigger", false);
        return true;
    }

private:
    Shape shape_;
    bool is2D_ = false;
    bool isTrigger_ = false;

    std::function<void(const HitInfo3D &)> onCollisionEnter3D_;
    std::function<void(const HitInfo3D &)> onCollisionStay3D_;
    std::function<void(const HitInfo3D &)> onCollisionExit3D_;

    std::function<void(const HitInfo2D &)> onCollisionEnter2D_;
    std::function<void(const HitInfo2D &)> onCollisionStay2D_;
    std::function<void(const HitInfo2D &)> onCollisionExit2D_;
};

} // namespace KashipanEngine
