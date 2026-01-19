#pragma once

#include "Objects/IObjectComponent.h"
#include "Objects/ObjectContext.h"

#include "Input/InputCommand.h"
#include "Math/Vector3.h"
#include "Objects/Components/3D/Transform3D.h"
#include "Utilities/TimeUtils.h"

#include <algorithm>
#include <functional>
#include <memory>

namespace KashipanEngine {

class PlayerMovement final : public IObjectComponent3D {
public:
    static const std::string &GetStaticComponentType() {
        static const std::string type = "PlayerMovement";
        return type;
    }

    /// @brief 入力コマンドと移動速度を指定してコンポーネントを作成する
    /// @param inputCommand 入力評価用の InputCommand
    /// @param moveSpeed 基本移動速度
    /// @param conditionFunc 更新を許可する条件関数（省略可）
    explicit PlayerMovement(
        const InputCommand *inputCommand,
        float moveSpeed = 5.0f,
        std::function<bool()> conditionFunc = {})
        : IObjectComponent3D(GetStaticComponentType(), 1),
          inputCommand_(inputCommand),
          moveSpeed_(moveSpeed),
          conditionFunc_(std::move(conditionFunc)) {
    }

    /// @brief デストラクタ
    ~PlayerMovement() override = default;

    /// @brief コンポーネントを複製して返す
    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<PlayerMovement>(inputCommand_, moveSpeed_, conditionFunc_);
        ptr->acceleration_ = acceleration_;
        ptr->deceleration_ = deceleration_;
        ptr->dashSpeed_ = dashSpeed_;
        ptr->dashCooldownSec_ = dashCooldownSec_;
        ptr->dashCooldownRemaining_ = dashCooldownRemaining_;
        ptr->isDashing_ = isDashing_;
        ptr->velocity_ = velocity_;
        ptr->boundsEnabled_ = boundsEnabled_;
        ptr->minBounds_ = minBounds_;
        ptr->maxBounds_ = maxBounds_;
        ptr->isMoving_ = isMoving_;
        ptr->justDodgeWindowSec_ = justDodgeWindowSec_;
        ptr->sinceDashTriggeredSec_ = sinceDashTriggeredSec_;
        return ptr;
    }

    /// @brief 毎フレームの更新。入力に基づいて移動やダッシュを処理する
    /// @return 更新成功時は true
    std::optional<bool> Update() override {
        isMoving_ = false;
        isDashing_ = false;

        if (!inputCommand_) return false;
        if (conditionFunc_ && !conditionFunc_()) return true;

        Transform3D *tr = GetOwner3DContext()->GetComponent<Transform3D>();
        if (!tr) return false;

        const float dt = std::max(0.0f, GetDeltaTime());

        // dash trigger からの経過時間
        if (sinceDashTriggeredSec_ >= 0.0f) {
            sinceDashTriggeredSec_ += dt;
        }

        float forward = 0.0f;
        float right = 0.0f;

        {
            const auto r = inputCommand_->Evaluate("MoveZ");
            if (r.Triggered()) forward += r.Value();
        }
        {
            const auto r = inputCommand_->Evaluate("MoveX");
            if (r.Triggered()) right += r.Value();
        }

        Vector3 inputDir{ right, 0.0f, forward };
        const float dirLen = inputDir.Length();
        if (dirLen > 0.0001f) {
            inputDir = inputDir / dirLen;
            isMoving_ = true;
        } else {
            inputDir = Vector3{0.0f, 0.0f, 0.0f};
        }

        if (dashCooldownRemaining_ > 0.0f) {
            dashCooldownRemaining_ = std::max(0.0f, dashCooldownRemaining_ - dt);
        }

        const float targetVx = inputDir.x * moveSpeed_;
        const float targetVz = inputDir.z * moveSpeed_;

        const auto approach = [](float current, float target, float maxDelta) {
            if (current < target) {
                const float next = current + maxDelta;
                return (next > target) ? target : next;
            }
            if (current > target) {
                const float next = current - maxDelta;
                return (next < target) ? target : next;
            }
            return current;
        };

        if (dirLen > 0.0001f) {
            velocity_.x = approach(velocity_.x, targetVx, acceleration_ * dt);
            velocity_.z = approach(velocity_.z, targetVz, acceleration_ * dt);
        } else {
            // 入力が無い場合も減速は常に行う
            velocity_.x = approach(velocity_.x, 0.0f, deceleration_ * dt);
            velocity_.z = approach(velocity_.z, 0.0f, deceleration_ * dt);
        }

        const auto dashCmd = inputCommand_->Evaluate("Dash");
        if (dirLen > 0.0001f && dashCmd.Triggered() && dashCooldownRemaining_ <= 0.0f) {
            velocity_.x = inputDir.x * dashSpeed_;
            velocity_.z = inputDir.z * dashSpeed_;
            dashCooldownRemaining_ = dashCooldownSec_;

            sinceDashTriggeredSec_ = 0.0f;

            auto soundHandle = AudioManager::GetSoundHandleFromFileName("avoid.mp3");
            AudioManager::Play(soundHandle, 1.0f, 0.0f, false);
        }
        if (dashCooldownRemaining_ > 0.0f) {
            isDashing_ = true;
        }

        if (velocity_.x != 0.0f || velocity_.z != 0.0f) {
            Vector3 pos = tr->GetTranslate();
            pos = pos + Vector3{velocity_.x, 0.0f, velocity_.z} * dt;

            // 移動範囲制限（XZ）
            if (boundsEnabled_) {
                pos.x = std::clamp(pos.x, minBounds_.x, maxBounds_.x);
                pos.z = std::clamp(pos.z, minBounds_.z, maxBounds_.z);
            }

            tr->SetTranslate(pos);
        }

        return true;
    }

    /// @brief 更新を許可する条件関数を設定する
    /// @param func 条件関数
    void SetConditionFunc(std::function<bool()> func) { conditionFunc_ = std::move(func); }
    /// @brief 移動速度を設定する
    /// @param s 移動速度
    void SetMoveSpeed(float s) { moveSpeed_ = s; }
    /// @brief 移動速度を取得する
    float GetMoveSpeed() const { return moveSpeed_; }

    /// @brief 加速度を設定する
    /// @param a 加速度
    void SetAcceleration(float a) { acceleration_ = a; }
    /// @brief 加速度を取得する
    float GetAcceleration() const { return acceleration_; }

    /// @brief 減速度を設定する
    /// @param d 減速度
    void SetDeceleration(float d) { deceleration_ = d; }
    /// @brief 減速度を取得する
    float GetDeceleration() const { return deceleration_; }

    /// @brief ダッシュ速度を設定する
    /// @param s ダッシュ速度
    void SetDashSpeed(float s) { dashSpeed_ = s; }
    /// @brief ダッシュ速度を取得する
    float GetDashSpeed() const { return dashSpeed_; }

    /// @brief ダッシュのクールダウン秒数を設定する
    /// @param s クールダウン秒数
    void SetDashCooldownSec(float s) { dashCooldownSec_ = s; }
    /// @brief ダッシュのクールダウン秒数を取得する
    float GetDashCooldownSec() const { return dashCooldownSec_; }

    /// @brief ダッシュのクールダウン残り時間を取得する
    float GetDashCooldownRemaining() const { return dashCooldownRemaining_; }

    /// @brief 移動中かどうかを取得する
    bool IsMoving() const { return isMoving_; }
    /// @brief ダッシュ中かどうかを取得する
    bool IsDashing() const { return isDashing_; }

    /// @brief JustDodge ウィンドウの長さを設定する
    /// @param s ウィンドウ長（秒）
    void SetJustDodgeWindowSec(float s) { justDodgeWindowSec_ = std::max(0.0f, s); }
    /// @brief JustDodge ウィンドウの長さを取得する
    float GetJustDodgeWindowSec() const { return justDodgeWindowSec_; }

    /// @brief 現在 JustDodge ウィンドウ内かを取得する
    bool IsJustDodging() const {
        return (sinceDashTriggeredSec_ >= 0.0f) && (sinceDashTriggeredSec_ <= justDodgeWindowSec_);
    }

    // 移動範囲を設定する（XZのみ使用）
    /// @brief XZ 方向の移動制限を設定する
    /// @param minV 最小座標
    /// @param maxV 最大座標
    void SetBoundsXZ(const Vector3 &minV, const Vector3 &maxV) {
        minBounds_ = minV;
        maxBounds_ = maxV;
        boundsEnabled_ = true;
    }

    /// @brief 移動制限を解除する
    void ClearBounds() { boundsEnabled_ = false; }

    /// @brief 最小移動制限を取得する
    const Vector3 &GetMinBounds() const { return minBounds_; }
    /// @brief 最大移動制限を取得する
    const Vector3 &GetMaxBounds() const { return maxBounds_; }
    /// @brief 現在の速度を取得する
    const Vector3 &GetVelocity() const { return velocity_; }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

private:
    Transform3D *GetTransform() const {
        Object3DContext *ctx = GetOwner3DContext();
        if (!ctx) return nullptr;

        auto comps = ctx->GetComponents("Transform3D");
        if (comps.empty() || !comps[0]) return nullptr;
        return static_cast<Transform3D *>(comps[0]);
    }

    const InputCommand *inputCommand_ = nullptr;

    float moveSpeed_ = 5.0f;
    float acceleration_ = 20.0f;
    float deceleration_ = 25.0f;

    float dashSpeed_ = 12.0f;
    float dashCooldownSec_ = 0.5f;
    float dashCooldownRemaining_ = 0.0f;

    Vector3 velocity_{0.0f, 0.0f, 0.0f};

    // 移動範囲制限（ワールド座標 / XZ）
    bool boundsEnabled_ = false;
    Vector3 minBounds_{0.0f, 0.0f, 0.0f};
    Vector3 maxBounds_{0.0f, 0.0f, 0.0f};

    std::function<bool()> conditionFunc_{};

    bool isMoving_ = false;
    bool isDashing_ = false;

    float justDodgeWindowSec_ = 0.2f;
    float sinceDashTriggeredSec_ = -1.0f;
};

} // namespace KashipanEngine
