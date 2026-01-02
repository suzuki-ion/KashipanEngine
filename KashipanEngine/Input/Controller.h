#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace KashipanEngine {

class Controller {
public:
    struct PadState {
        std::uint16_t buttons = 0; // XINPUT_GAMEPAD_* compatible mask
        std::uint8_t leftTrigger = 0;
        std::uint8_t rightTrigger = 0;
        std::int16_t leftX = 0;
        std::int16_t leftY = 0;
        std::int16_t rightX = 0;
        std::int16_t rightY = 0;
    };

    Controller();
    ~Controller();

    Controller(const Controller&) = delete;
    Controller(Controller&&) = delete;
    Controller& operator=(const Controller&) = delete;
    Controller& operator=(Controller&&) = delete;

    void Initialize();
    void Finalize();
    void Update();

    /// @brief 現在エンジンが把握しているゲームパッド一覧を取得（インデックスはこの配列準拠）
    std::span<const PadState> GetPads() const noexcept { return current_; }

    /// @brief 直前フレームのゲームパッド一覧を取得（インデックスは GetPads() と同じ）
    std::span<const PadState> GetPrevPads() const noexcept { return previous_; }

    /// @brief GetPads() の要素数（接続/把握しているゲームパッド数）を取得
    int GetPadCount() const noexcept { return static_cast<int>(current_.size()); }

    /// @brief 指定インデックスのコントローラーが接続されているかを取得
    bool IsConnected(int index) const;

    /// @brief 前フレームで指定インデックスのコントローラーが接続されていたかを取得
    bool WasConnected(int index) const;

    /// @brief 指定ボタンが押されているかを取得
    bool IsButtonDown(int button, int index) const;
    /// @brief 指定ボタンが押された瞬間か（トリガー）を取得
    bool IsButtonTrigger(int button, int index) const;
    /// @brief 指定ボタンが離された瞬間か（リリース）を取得
    bool IsButtonRelease(int button, int index) const;
    /// @brief 前フレームで指定ボタンが押されていたかを取得
    bool WasButtonDown(int button, int index) const;

    /// @brief 左トリガーの押し込み量（0-255）を取得
    int GetLeftTrigger(int index) const;
    /// @brief 右トリガーの押し込み量（0-255）を取得
    int GetRightTrigger(int index) const;

    /// @brief 左スティックX（-32767～32767）を取得
    int GetLeftStickX(int index) const;
    /// @brief 左スティックY（-32767～32767）を取得
    int GetLeftStickY(int index) const;
    /// @brief 右スティックX（-32767～32767）を取得
    int GetRightStickX(int index) const;
    /// @brief 右スティックY（-32767～32767）を取得
    int GetRightStickY(int index) const;

    /// @brief 振動を設定
    /// @details leftMotor/rightMotor は 0～65535 の強さ（XInput 互換の想定）
    void SetVibration(int index, int leftMotor, int rightMotor);
    /// @brief 振動を停止
    void StopVibration(int index);

    /// @brief 前フレームの左トリガー押し込み量（0-255）を取得
    int GetPrevLeftTrigger(int index) const;
    /// @brief 前フレームの右トリガー押し込み量（0-255）を取得
    int GetPrevRightTrigger(int index) const;
    /// @brief 前フレームの左スティックX（-32767～32767）を取得
    int GetPrevLeftStickX(int index) const;
    /// @brief 前フレームの左スティックY（-32767～32767）を取得
    int GetPrevLeftStickY(int index) const;
    /// @brief 前フレームの右スティックX（-32767～32767）を取得
    int GetPrevRightStickX(int index) const;
    /// @brief 前フレームの右スティックY（-32767～32767）を取得
    int GetPrevRightStickY(int index) const;

    /// @brief 左トリガーのフレーム差分（current - previous）を取得
    int GetDeltaLeftTrigger(int index) const;
    /// @brief 右トリガーのフレーム差分（current - previous）を取得
    int GetDeltaRightTrigger(int index) const;
    /// @brief 左スティックXのフレーム差分（current - previous）を取得
    int GetDeltaLeftStickX(int index) const;
    /// @brief 左スティックYのフレーム差分（current - previous）を取得
    int GetDeltaLeftStickY(int index) const;
    /// @brief 右スティックXのフレーム差分（current - previous）を取得
    int GetDeltaRightStickX(int index) const;
    /// @brief 右スティックYのフレーム差分（current - previous）を取得
    int GetDeltaRightStickY(int index) const;

private:
    static std::uint16_t ButtonsToXInputMask_(std::uint32_t gameInputButtons) noexcept;

    std::vector<PadState> current_;
    std::vector<PadState> previous_;
    std::vector<bool> connected_;
    std::vector<bool> prevConnected_;

    std::int16_t stickDeadZone_ = 4096;

public: // internal (used by GameInput callback trampoline in .cpp)
    struct DeviceEntry {
        void* device = nullptr; // actually IGameInputDevice*
    };

    void OnDeviceChanged_(void* device, std::uint32_t currentStatus);

private:
    std::vector<DeviceEntry> devices_;

    std::uint64_t deviceCallbackToken_ = 0;
};

} // namespace KashipanEngine
