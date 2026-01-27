#pragma once
#include <KashipanEngine.h>
#include "Scenes/Components/CameraController.h"

namespace KashipanEngine {

/// HPを管理するコンポーネント
class Health final : public IObjectComponent3D {
public:
    explicit Health(int hp, float damageCooldownSec = 0.5f)
        : IObjectComponent3D("Health", 1), hp_(hp), damageCooldownSec_(damageCooldownSec) {}

    ~Health() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Health>(hp_, damageCooldownSec_);
        ptr->isAlive_ = isAlive_;
        ptr->wasDamaged_ = wasDamaged_;
        ptr->cooldownRemaining_ = cooldownRemaining_;
        ptr->cameraController_ = cameraController_;
        ptr->blinkInterval_ = blinkInterval_;
        ptr->blinkTimer_ = blinkTimer_;
        return ptr;
    }

    std::optional<bool> Update() override {
        if (!isAlive_) return true;

        const float dt = std::max(0.0f, GetDeltaTime());
        if (cooldownRemaining_ > 0.0f) {
            cooldownRemaining_ = std::max(0.0f, cooldownRemaining_ - dt);
            
            // 点滅タイマーの更新
            blinkTimer_ += dt;
        }

        UpdateMaterialTint();
        return true;
    }

    void Damage(int amount = 1) {
        if (!isAlive_) return;
        if (amount <= 0) return;
        if (cooldownRemaining_ > 0.0f) return;

        hp_ -= amount;
        wasDamaged_ = true;
        cooldownRemaining_ = damageCooldownSec_;
        blinkTimer_ = 0.0f; // 点滅タイマーをリセット

        if (hp_ <= 0) {
            hp_ = 0;
            isAlive_ = false;
        }

        // ダメージを受けた時にカメラをシェイク
        if (cameraController_) {
            cameraController_->Shake(shakePower_, shakeTime_);
        }

        UpdateMaterialTint();
    }

    int GetHp() const { return hp_; }
    bool IsAlive() const { return isAlive_; }
    bool WasDamagedThisCooldown() const { return wasDamaged_ && cooldownRemaining_ > 0.0f; }

    float GetDamageCooldownRemaining() const { return cooldownRemaining_; }

    /// @brief カメラコントローラーを設定
    void SetCameraController(CameraController* cameraController) { cameraController_ = cameraController; }

    /// @brief 点滅間隔を設定（秒）
    void SetBlinkInterval(float interval) { blinkInterval_ = interval; }

    void SetShake(float power, float time) {
        shakePower_ = power;
        shakeTime_ = time;
	}
#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::TextUnformatted("Health");
        ImGui::Text("HP: %d", hp_);
        ImGui::Text("Is Alive: %s", isAlive_ ? "Yes" : "No");
        ImGui::DragFloat("Blink Interval", &blinkInterval_, 0.01f, 0.01f, 1.0f);
        if (cooldownRemaining_ > 0.0f) {
            ImGui::Text("Damage Cooldown: %.2f", cooldownRemaining_);
        }
    }
#endif

private:
    void UpdateMaterialTint() {
        auto *ctx = GetOwner3DContext();
        if (!ctx) return;

        auto *mat = ctx->GetComponent<Material3D>();
        if (!mat) return;

        if (cooldownRemaining_ > 0.0f) {
                // 点滅処理：blinkInterval_ごとに色を切り替える
            float phase = std::fmod(blinkTimer_, blinkInterval_ * 2.0f);
            if (phase < blinkInterval_) {
                // 赤色
                mat->SetColor(Vector4{1.0f, 0.0f, 0.0f, 1.0f});
            } else {
                // 通常色
                mat->SetColor(Vector4{1.0f, 1.0f, 1.0f, 0.0f});
            }
        } else {
            mat->SetColor(Vector4{1.0f, 1.0f, 1.0f, 1.0f});
            wasDamaged_ = false;
            blinkTimer_ = 0.0f;
        }
    }

    int hp_ = 0;
    bool isAlive_ = true;
    bool wasDamaged_ = false;

    float damageCooldownSec_ = 0.5f;
    float cooldownRemaining_ = 0.0f;

    // 点滅設定
    float blinkInterval_ = 0.1f; // 0.1秒ごとに点滅
    float blinkTimer_ = 0.0f;

	float shakePower_ = 5.0f;
	float shakeTime_ = 1.0f;

    CameraController* cameraController_ = nullptr;
};

} // namespace KashipanEngine
