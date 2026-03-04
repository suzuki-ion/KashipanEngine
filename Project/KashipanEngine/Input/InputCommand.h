#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

#include "Input/ControllerButton.h"
#include "Input/Key.h"
#include "Input/MouseButton.h"
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class GameEngine;
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

    struct ReturnInfo {
    private:
        friend class InputCommand;

        bool triggered = false;
        float value = 0.0f;

        ReturnInfo(bool triggered, float value)
            : triggered(triggered), value(value) {}

    public:
        bool Triggered() const noexcept { return triggered; }
        float Value() const noexcept { return value; }
    };

    InputCommand() = delete;
    InputCommand(Passkey<GameEngine>, const Input* input);

    /// @brief すべてのコマンド登録をクリア
    void Clear();

    /// @brief コマンドを登録する（キーボード）
    /// @param action コマンド名
    /// @param key キー（KashipanEngine::Key）
    /// @param state 入力状態（押下、トリガー、リリース）
    /// @param invertValue 評価値(value)を反転させるかどうか
    void RegisterCommand(const std::string& action, Key key, InputState state, bool invertValue = false);

    /// @brief コマンドを登録する（マウスボタン）
    /// @param action コマンド名
    /// @param button マウスボタン（KashipanEngine::MouseButton）
    /// @param state 入力状態（押下、トリガー、リリース）
    /// @param invertValue 評価値(value)を反転させるかどうか
    void RegisterCommand(const std::string& action, MouseButton button, InputState state, bool invertValue = false);

    /// @brief コマンドを登録する（マウス軸）
    /// @details `axis` によって座標/移動量/ホイール等を登録できる。
    ///          `hwnd` を指定した場合はクライアント座標系（Client）として扱う。
    /// @param action コマンド名
    /// @param axis 取得するマウス軸（X/Y/DeltaX/DeltaY/Wheel/DeltaWheel）
    /// @param hwnd クライアント座標系で扱う場合の HWND（Screen 座標系の場合は nullptr）
    /// @param threshold 軸入力の閾値（絶対値が閾値を超えたら triggered=true）
    /// @param invertValue 評価値(value)を反転させるかどうか
    void RegisterCommand(const std::string& action, MouseAxis axis, void* hwnd = nullptr, float threshold = 0.0f, bool invertValue = false);

    /// @brief コマンドを登録する（コントローラーボタン）
    /// @param action コマンド名
    /// @param button ボタン
    /// @param state 入力状態（押下、トリガー、リリース）
    /// @param controllerIndex コントローラーインデックス（0-3）
    /// @param invertValue 評価値(value)を反転させるかどうか
    void RegisterCommand(const std::string& action, ControllerButton button, InputState state, int controllerIndex = 0, bool invertValue = false);

    /// @brief コマンドを登録する（コントローラーアナログ）
    /// @details トリガーは 0.0f～1.0f、スティックは -1.0f～1.0f に正規化して返す。
    /// @param action コマンド名
    /// @param analog 取得するアナログ入力種別（トリガー/スティック）
    /// @param state 入力状態（Down/Trigger/Release）
    /// @param controllerIndex コントローラーインデックス（0-3）
    /// @param threshold アナログ入力の閾値（絶対値が閾値を超えたら Down/Trigger 判定を true）
    /// @param invertValue 評価値(value)を反転させるかどうか
    void RegisterCommand(const std::string& action, ControllerAnalog analog, InputState state, int controllerIndex = 0, float threshold = 0.0f, bool invertValue = false);

    /// @brief コマンドを登録する（コントローラーアナログ差分）
    /// @details current - previous の差分を -1.0f～1.0f 相当に正規化して返す。
    /// @param action コマンド名
    /// @param analog 取得するアナログ入力種別（トリガー/スティック）
    /// @param controllerIndex コントローラーインデックス（0-3）
    /// @param threshold 差分入力の閾値（絶対値が閾値を超えたら triggered=true）
    /// @param invertValue 評価値(value)を反転させるかどうか
    void RegisterCommand(const std::string& action, ControllerAnalog analog, int controllerIndex = 0, float threshold = 0.0f, bool invertValue = false);

    /// @brief コマンドを評価する
    /// @param action コマンド名
    /// @return 入力評価結果
    ReturnInfo Evaluate(const std::string& action) const;

    /// @brief 登録済みコマンドをJSONファイルに保存する
    /// @param filepath 保存先ファイルパス
    /// @return 保存に成功した場合 true
    bool SaveToJSON(const std::string& filepath) const;

    /// @brief JSONファイルからコマンドを読み込んで登録する
    /// @details 読み込み前に既存のコマンドはクリアされる。
    /// @param filepath 読み込み元ファイルパス
    /// @return 読み込みに成功した場合 true
    bool LoadFromJSON(const std::string& filepath);

    // --- 文字列 ↔ 列挙型変換ユーティリティ ---

    static std::string KeyToString(Key key);
    static Key StringToKey(const std::string& str);

    static std::string MouseButtonToString(MouseButton button);
    static MouseButton StringToMouseButton(const std::string& str);

    static std::string ControllerButtonToString(ControllerButton button);
    static ControllerButton StringToControllerButton(const std::string& str);

    static std::string ControllerAnalogToString(ControllerAnalog analog);
    static ControllerAnalog StringToControllerAnalog(const std::string& str);

    static std::string InputStateToString(InputState state);
    static InputState StringToInputState(const std::string& str);

    static std::string MouseAxisToString(MouseAxis axis);
    static MouseAxis StringToMouseAxis(const std::string& str);

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
        int code = 0;              // mouse/controller only
        Key key = Key::Unknown;    // keyboard only
        int controllerIndex = 0;   // コントローラーインデックス（コントローラー用のみ）
        float threshold = 0.0f;    // アナログ入力の閾値（アナログ用のみ）
        bool invertValue = false;  // 評価値(value)を反転させるかどうか
        ControllerButton controllerButton = ControllerButton::A;

        // マウス軸用パラメータ
        MouseAxis mouseAxis = MouseAxis::X;
        MouseSpace mouseSpace = MouseSpace::Screen;
        void* hwnd = nullptr;      // Client 座標系で使う場合の HWND
    };

    const Input* input_ = nullptr;
    std::unordered_map<std::string, std::vector<Binding>> bindings_;

    ReturnInfo EvaluateBinding(const Binding& b) const;

    static ReturnInfo MakeReturnInfo(bool triggered, float value) { return ReturnInfo(triggered, value); }

    static std::string DeviceKindToString(DeviceKind kind);
    static DeviceKind StringToDeviceKind(const std::string& str);
};

} // namespace KashipanEngine
