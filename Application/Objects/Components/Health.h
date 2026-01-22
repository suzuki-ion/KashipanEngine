#pragma once

#include "Objects/IObjectComponent.h"
#include "Objects/ObjectContext.h"
#include "Objects/Components/3D/Material3D.h"
#include "Utilities/TimeUtils.h"
#include "Math/Vector4.h"

#include <algorithm>
#include <cstdint>
#include <optional>

#include "Scenes/Components/CameraController.h"

namespace KashipanEngine {

// Forward declaration
class CameraController;

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
        return ptr;
    }

    std::optional<bool> Update() override {
        if (!isAlive_) return true;

        const float dt = std::max(0.0f, GetDeltaTime());
        if (cooldownRemaining_ > 0.0f) {
            cooldownRemaining_ = std::max(0.0f, cooldownRemaining_ - dt);
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

        if (hp_ <= 0) {
            hp_ = 0;
            isAlive_ = false;
        }

        // ダメージを受けた時にカメラをシェイク
        if (cameraController_) {
            cameraController_->Shake(5.0f, 1.0f);
        }

        UpdateMaterialTint();
    }

    int GetHp() const { return hp_; }
    bool IsAlive() const { return isAlive_; }
    bool WasDamagedThisCooldown() const { return wasDamaged_ && cooldownRemaining_ > 0.0f; }

    float GetDamageCooldownRemaining() const { return cooldownRemaining_; }

    /// @brief カメラコントローラーを設定
    void SetCameraController(CameraController* cameraController) { cameraController_ = cameraController; }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

private:
    void UpdateMaterialTint() {
        auto *ctx = GetOwner3DContext();
        if (!ctx) return;

        auto *mat = ctx->GetComponent<Material3D>();
        if (!mat) return;

        if (cooldownRemaining_ > 0.0f) {
            mat->SetColor(Vector4{1.0f, 0.0f, 0.0f, 1.0f});
        } else {
            mat->SetColor(Vector4{1.0f, 1.0f, 1.0f, 1.0f});
            wasDamaged_ = false;
        }
    }

    int hp_ = 0;
    bool isAlive_ = true;
    bool wasDamaged_ = false;

    float damageCooldownSec_ = 0.5f;
    float cooldownRemaining_ = 0.0f;

    CameraController* cameraController_ = nullptr;
};

} // namespace KashipanEngine
