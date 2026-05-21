#pragma once

#include <KashipanEngine.h>

#include <algorithm>

namespace KashipanEngine {

class PlayerActionGamepadVibrator final : public IObjectComponent3D {
public:
    PlayerActionGamepadVibrator() : IObjectComponent3D("PlayerActionGamepadVibrator", 1) {}
    ~PlayerActionGamepadVibrator() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        return std::make_unique<PlayerActionGamepadVibrator>();
    }

    void SetController(Controller *controller) {
        controller_ = controller;
    }

    void RequestLanding(float fallDistance) {
        const float t = Normalize01(fallDistance, landingMinImpact_, landingMaxImpact_);
        const float strength = std::clamp(Lerp(landingMinStrength_, landingMaxStrength_, t), 0.0f, 1.0f);
        const float landingDuration = std::clamp(Lerp(landingMinDuration_, landingMaxDuration_, t), 0.0f, 5.0f);
        StartLerpVibration(strength, landingDuration);
    }

    void RequestRespawn() {
        StartLerpVibration(respawnStrength_, respawnDuration_);
    }

    void RequestDeath() {
        StartLerpVibration(deathStrength_, deathDuration_);
    }

    void RequestCoin() {
        StartConstantVibration(coinStrength_, coinDuration_);
    }

    void RequestJump() {
        StartLerpVibration(jumpStrength_, jumpDuration_);
    }

    std::optional<bool> Update() override {
        if (!isActive_) return std::nullopt;

        const float dt = std::max(0.0f, GetDeltaTime() * GetGameSpeed());
        elapsed_ = std::min(duration_, elapsed_ + dt);

        if (isConstant_) {
            strength_ = constantStrength_;
        } else {
            const float t = (duration_ > 0.0f) ? std::clamp(elapsed_ / duration_, 0.0f, 1.0f) : 1.0f;
            strength_ = std::clamp(Lerp(startStrength_, 0.0f, t), 0.0f, 1.0f);
        }

        ApplyVibration(strength_);

        if (elapsed_ >= duration_) {
            StopVibration();
        }

        return true;
    }

    std::optional<bool> Finalize() override {
        StopVibration();
        return true;
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

private:
    void StartLerpVibration(float strength, float duration) {
        if (isActive_ && strength_ > strength) {
            // すでに強い振動が発生している場合は新しい振動を無視する
            return;
        }
        if (strength <= 0.0f || duration <= 0.0f) {
            // 強さまたは時間が無効な場合は振動を開始しない
            return;
        }

        isActive_ = true;
        isConstant_ = false;
        elapsed_ = 0.0f;
        duration_ = std::max(0.0f, duration);
        startStrength_ = std::clamp(strength, 0.0f, 1.0f);
    }

    void StartConstantVibration(float strength, float duration) {
        if (isActive_ && constantStrength_ > strength) {
            // すでに強い振動が発生している場合は新しい振動を無視する
            return;
        }
        if (strength <= 0.0f || duration <= 0.0f) {
            // 強さまたは時間が無効な場合は振動を開始しない
            return;
        }

        isActive_ = true;
        isConstant_ = true;
        elapsed_ = 0.0f;
        duration_ = std::max(0.0f, duration);
        constantStrength_ = std::clamp(strength, 0.0f, 1.0f);
    }

    void ApplyVibration(float strength) {
        if (!controller_) return;
        if (!controller_->IsConnected(padIndex_)) return;

        const int motor = static_cast<int>(std::clamp(strength, 0.0f, 1.0f) * 65535.0f);
        controller_->SetVibration(padIndex_, motor, motor);
    }

    void StopVibration() {
        if (!isActive_) return;

        isActive_ = false;
        isConstant_ = false;
        elapsed_ = 0.0f;
        duration_ = 0.0f;
        startStrength_ = 0.0f;
        constantStrength_ = 0.0f;

        if (!controller_) return;

        controller_->StopVibration(padIndex_);
    }

    int padIndex_ = 0;
    Controller *controller_ = nullptr;

    bool isActive_ = false;
    bool isConstant_ = false;
    float elapsed_ = 0.0f;
    float duration_ = 0.0f;
    float strength_ = 0.0f;
    float startStrength_ = 0.0f;
    float constantStrength_ = 0.0f;

    float landingMinDuration_ = 0.25f;
    float landingMaxDuration_ = 1.0f;
    float landingMinImpact_ = 8.0f;
    float landingMaxImpact_ = 64.0f;
    float landingMinStrength_ = 0.2f;
    float landingMaxStrength_ = 1.0f;

    float respawnDuration_ = 0.5f;
    float respawnStrength_ = 0.8f;

    float deathDuration_ = 0.5f;
    float deathStrength_ = 1.0f;

    float coinDuration_ = 0.0f;
    float coinStrength_ = 0.0f;

    float jumpDuration_ = 0.25f;
    float jumpStrength_ = 0.5f;
};

} // namespace KashipanEngine
