#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace KashipanEngine {

class Input;

class InputCommand {
public:
    enum class InputState {
        Down,
        Trigger,
        Release,
    };

    enum class ControllerAnalog {
        LeftTrigger,
        RightTrigger,
        LeftStickX,
        LeftStickY,
        RightStickX,
        RightStickY,
    };

    enum class MouseAxis {
        X,
        Y,
        DeltaX,
        DeltaY,
        Wheel,
        DeltaWheel,
    };

    enum class MouseSpace {
        Screen,
        Client,
    };

    struct KeyboardKey { int value = 0; };
    struct MouseButton { int value = 0; };
    struct ControllerButton { int value = 0; };

    struct ReturnInfo {
    private:
        friend class InputCommand;

        bool triggered_ = false;
        float value_ = 0.0f;

        ReturnInfo(bool triggered, float value)
            : triggered_(triggered), value_(value) {}

    public:
        bool Triggered() const noexcept { return triggered_; }
        float Value() const noexcept { return value_; }
    };

    InputCommand() = delete;
    explicit InputCommand(const Input* input);
    void SetInput(const Input* input);

    /// @brief すべてのコマンド登録をクリア
    void Clear();

    /// @brief コマンドを登録する
    /// @param action コマンド名
    /// @param key キーコード（DIK_*）
    /// @param state 入力状態（押下、トリガー、リリース）
    void RegisterCommand(const std::string& action, KeyboardKey key, InputState state);

    /// @brief コマンドを登録する（マウスボタン）
    /// @param action コマンド名
    /// @param button マウスボタン番号（0-7）
    /// @param state 入力状態（押下、トリガー、リリース）
    void RegisterCommand(const std::string& action, MouseButton button, InputState state);

    /// @brief コマンドを登録する（マウス軸）
    /// @details `axis` によって座標/移動量/ホイール等を登録できる。
    ///          `hwnd` を指定した場合はクライアント座標系（Client）として扱う。
    /// @param action コマンド名
    /// @param axis 取得するマウス軸（X/Y/DeltaX/DeltaY/Wheel/DeltaWheel）
    /// @param hwnd クライアント座標系で扱う場合の HWND（Screen 座標系の場合は nullptr）
    /// @param threshold 軸入力の閾値（絶対値が閾値を超えたら triggered=true）
    void RegisterCommand(const std::string& action, MouseAxis axis, void* hwnd = nullptr, float threshold = 0.0f);

    /// @brief コマンドを登録する（コントローラーボタン）
    /// @param action コマンド名
    /// @param button ボタンビット（XINPUT_GAMEPAD_*）
    /// @param state 入力状態（押下、トリガー、リリース）
    /// @param controllerIndex コントローラーインデックス（0-3）
    void RegisterCommand(const std::string& action, ControllerButton button, InputState state, int controllerIndex = 0);

    /// @brief コマンドを登録する（コントローラーアナログ）
    /// @details トリガーは 0.0f～1.0f、スティックは -1.0f～1.0f に正規化して返す。
    /// @param action コマンド名
    /// @param analog 取得するアナログ入力種別（トリガー/スティック）
    /// @param state 入力状態（Down/Trigger/Release）
    /// @param controllerIndex コントローラーインデックス（0-3）
    /// @param threshold アナログ入力の閾値（絶対値が閾値を超えたら Down/Trigger 判定を true）
    void RegisterCommand(const std::string& action, ControllerAnalog analog, InputState state, int controllerIndex = 0, float threshold = 0.0f);

    /// @brief コマンドを登録する（コントローラーアナログ差分）
    /// @details current - previous の差分を -1.0f～1.0f 相当に正規化して返す。
    /// @param action コマンド名
    /// @param analog 取得するアナログ入力種別（トリガー/スティック）
    /// @param controllerIndex コントローラーインデックス（0-3）
    /// @param threshold 差分入力の閾値（絶対値が閾値を超えたら triggered=true）
    void RegisterCommand(const std::string& action, ControllerAnalog analog, int controllerIndex = 0, float threshold = 0.0f);

    /// @brief コマンドを評価する
    /// @param action コマンド名
    /// @return 入力評価結果
    ReturnInfo Evaluate(const std::string& action) const;

private:
    enum class DeviceKind {
        Keyboard,
        MouseButton,
        MouseAxis,
        ControllerButton,
        ControllerAnalog,
        ControllerAnalogDelta,
    };

    struct Binding {
        DeviceKind kind{};
        InputState state = InputState::Down; // delta/axis では意味を持たない場合あり
        int code = 0;              // キーコード、ボタンコード、アナログコードなど
        int controllerIndex = 0;   // コントローラーインデックス（コントローラー用のみ）
        float threshold = 0.0f;    // アナログ入力の閾値（アナログ用のみ）

        // マウス軸用パラメータ
        MouseAxis mouseAxis = MouseAxis::X;
        MouseSpace mouseSpace = MouseSpace::Screen;
        void* hwnd = nullptr;      // Client 座標系で使う場合の HWND
    };

    const Input* input_ = nullptr;
    std::unordered_map<std::string, std::vector<Binding>> bindings_;

    ReturnInfo EvaluateBinding(const Binding& b) const;

    static ReturnInfo MakeReturnInfo(bool triggered, float value) { return ReturnInfo(triggered, value); }
};

} // namespace KashipanEngine
