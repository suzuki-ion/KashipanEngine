#pragma once

#include <cstdint>
#include <array>

struct _XINPUT_STATE;
struct _XINPUT_VIBRATION;
using XINPUT_STATE = _XINPUT_STATE;
using XINPUT_VIBRATION = _XINPUT_VIBRATION;

namespace KashipanEngine {

class Controller {
public:
    Controller();
    ~Controller();

    Controller(const Controller&) = delete;
    Controller(Controller&&) = delete;
    Controller& operator=(const Controller&) = delete;
    Controller& operator=(Controller&&) = delete;

    void Initialize();
    void Finalize();
    void Update();

    /// @brief 指定インデックスのコントローラーが接続中かを取得
    /// @param index コントローラーインデックス（0-3）
    /// @return 接続中の場合 true
    bool IsConnected(int index = 0) const;
    /// @brief 前フレームで指定インデックスのコントローラーが接続中だったかを取得
    /// @param index コントローラーインデックス（0-3）
    /// @return 前フレームで接続中だった場合 true
    bool WasConnected(int index = 0) const;

    /// @brief 指定ボタンが押されているかを取得
    /// @param button ボタンビット（XINPUT_GAMEPAD_*）
    /// @param index コントローラーインデックス（0-3）
    /// @return 押されている場合 true
    bool IsButtonDown(int button, int index = 0) const;
    /// @brief 指定ボタンが押された瞬間か（トリガー）を取得
    /// @param button ボタンビット（XINPUT_GAMEPAD_*）
    /// @param index コントローラーインデックス（0-3）
    /// @return 今フレームで押された場合 true
    bool IsButtonTrigger(int button, int index = 0) const;
    /// @brief 指定ボタンが離された瞬間か（リリース）を取得
    /// @param button ボタンビット（XINPUT_GAMEPAD_*）
    /// @param index コントローラーインデックス（0-3）
    /// @return 今フレームで離された場合 true
    bool IsButtonRelease(int button, int index = 0) const;
    /// @brief 前フレームで指定ボタンが押されていたかを取得
    /// @param button ボタンビット（XINPUT_GAMEPAD_*）
    /// @param index コントローラーインデックス（0-3）
    /// @return 前フレームで押されていた場合 true
    bool WasButtonDown(int button, int index = 0) const;

    /// @brief 左トリガー値を取得
    /// @param index コントローラーインデックス（0-3）
    /// @return 左トリガー値（0-255）
    int GetLeftTrigger(int index = 0) const;
    /// @brief 右トリガー値を取得
    /// @param index コントローラーインデックス（0-3）
    /// @return 右トリガー値（0-255）
    int GetRightTrigger(int index = 0) const;

    /// @brief 左スティックX値を取得
    /// @param index コントローラーインデックス（0-3）
    /// @return 左スティックX値
    int GetLeftStickX(int index = 0) const;
    /// @brief 左スティックY値を取得
    /// @param index コントローラーインデックス（0-3）
    /// @return 左スティックY値
    int GetLeftStickY(int index = 0) const;
    /// @brief 右スティックX値を取得
    /// @param index コントローラーインデックス（0-3）
    /// @return 右スティックX値
    int GetRightStickX(int index = 0) const;
    /// @brief 右スティックY値を取得
    /// @param index コントローラーインデックス（0-3）
    /// @return 右スティックY値
    int GetRightStickY(int index = 0) const;

    /// @brief 振動を設定
    /// @param index コントローラーインデックス（0-3）
    /// @param leftMotor 左モーター強度（0-65535相当）
    /// @param rightMotor 右モーター強度（0-65535相当）
    void SetVibration(int index, int leftMotor, int rightMotor);
    /// @brief 振動を停止
    /// @param index コントローラーインデックス（0-3）
    void StopVibration(int index);

    /// @brief 前フレームの左トリガー値を取得
    /// @param index コントローラーインデックス（0-3）
    /// @return 前フレームの左トリガー値（0-255）
    int GetPrevLeftTrigger(int index = 0) const;
    /// @brief 前フレームの右トリガー値を取得
    /// @param index コントローラーインデックス（0-3）
    /// @return 前フレームの右トリガー値（0-255）
    int GetPrevRightTrigger(int index = 0) const;
    /// @brief 前フレームの左スティックX値を取得
    /// @param index コントローラーインデックス（0-3）
    /// @return 前フレームの左スティックX値
    int GetPrevLeftStickX(int index = 0) const;
    /// @brief 前フレームの左スティックY値を取得
    /// @param index コントローラーインデックス（0-3）
    /// @return 前フレームの左スティックY値
    int GetPrevLeftStickY(int index = 0) const;
    /// @brief 前フレームの右スティックX値を取得
    /// @param index コントローラーインデックス（0-3）
    /// @return 前フレームの右スティックX値
    int GetPrevRightStickX(int index = 0) const;
    /// @brief 前フレームの右スティックY値を取得
    /// @param index コントローラーインデックス（0-3）
    /// @return 前フレームの右スティックY値
    int GetPrevRightStickY(int index = 0) const;

    /// @brief トリガー値の差分を取得
    /// @param index コントローラーインデックス（0-3）
    /// @return トリガー値の差分
    int GetDeltaLeftTrigger(int index = 0) const;
    /// @brief トリガー値の差分を取得
    /// @param index コントローラーインデックス（0-3）
    /// @return トリガー値の差分
    int GetDeltaRightTrigger(int index = 0) const;
    /// @brief スティックX値の差分を取得
    /// @param index コントローラーインデックス（0-3）
    /// @return スティックX値の差分
    int GetDeltaLeftStickX(int index = 0) const;
    /// @brief スティックY値の差分を取得
    /// @param index コントローラーインデックス（0-3）
    /// @return スティックY値の差分
    int GetDeltaLeftStickY(int index = 0) const;
    /// @brief スティックX値の差分を取得
    /// @param index コントローラーインデックス（0-3）
    /// @return スティックX値の差分
    int GetDeltaRightStickX(int index = 0) const;
    /// @brief スティックY値の差分を取得
    /// @param index コントローラーインデックス（0-3）
    /// @return スティックY値の差分
    int GetDeltaRightStickY(int index = 0) const;

private:
    std::array<XINPUT_STATE, 4>* current_ = nullptr;
    std::array<XINPUT_STATE, 4>* previous_ = nullptr;
    std::array<bool, 4>* connected_ = nullptr;
    std::array<bool, 4>* prevConnected_ = nullptr;
    std::array<XINPUT_VIBRATION, 4>* vibration_ = nullptr;

    std::int16_t stickDeadZone_ = 4096;
};

} // namespace KashipanEngine
