#pragma once

#include <chrono>
#include <memory>
#include <windows.h>

#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class GameEngine;
class Keyboard;
class Mouse;
class Controller;

/// @brief 現在入力に使われているデバイスの種別
enum class InputDeviceType {
    KeyboardMouse,
    Controller,
};

class Input {
public:
    Input(Passkey<GameEngine>);
    ~Input();

    Input(const Input&) = delete;
    Input(Input&&) = delete;
    Input& operator=(const Input&) = delete;
    Input& operator=(Input&&) = delete;

    void Update();

    /// @brief キーボード入力インスタンスを取得
    /// @return `Keyboard` への参照
    Keyboard& GetKeyboard();
    /// @brief キーボード入力インスタンスを取得（const）。
    /// @return `Keyboard` への参照
    const Keyboard& GetKeyboard() const;

    /// @brief マウス入力インスタンスを取得
    /// @return `Mouse` への参照
    Mouse& GetMouse();
    /// @brief マウス入力インスタンスを取得（const）。
    /// @return `Mouse` への参照
    const Mouse& GetMouse() const;

    /// @brief コントローラー入力インスタンスを取得
    /// @return `Controller` への参照
    Controller& GetController();
    /// @brief コントローラー入力インスタンスを取得（const）。
    /// @return `Controller` への参照
    const Controller& GetController() const;

    /// @brief 最後に入力があったデバイスの種別を取得
    /// @details どのデバイスにも入力がないフレームでは直前の判定結果を維持する。
    ///          起動直後は `InputDeviceType::KeyboardMouse`。
    InputDeviceType GetActiveDevice() const noexcept { return activeDevice_; }
    /// @brief 現在の入力デバイスがキーボード＋マウスかを取得
    bool IsUsingKeyboardMouse() const noexcept { return activeDevice_ == InputDeviceType::KeyboardMouse; }
    /// @brief 現在の入力デバイスがコントローラーかを取得
    bool IsUsingController() const noexcept { return activeDevice_ == InputDeviceType::Controller; }
    /// @brief このフレームで入力デバイスが切り替わったかを取得
    bool IsActiveDeviceChanged() const noexcept { return activeDeviceChanged_; }
    /// @brief 最後に入力があったコントローラーのインデックスを取得（未操作なら -1）
    int GetActiveControllerIndex() const noexcept { return activeControllerIndex_; }

    /// @brief マウス移動をデバイス切り替えとみなす累積移動量（ピクセル）を設定
    void SetMouseMoveThreshold(float pixels) noexcept;
    /// @brief マウス移動をデバイス切り替えとみなす累積移動量（ピクセル）を取得
    float GetMouseMoveThreshold() const noexcept { return mouseMoveThreshold_; }
    /// @brief スティック入力をデバイス切り替えとみなす傾き（0.0～1.0）を設定
    void SetControllerStickThreshold(float threshold) noexcept;
    /// @brief スティック入力をデバイス切り替えとみなす傾き（0.0～1.0）を取得
    float GetControllerStickThreshold() const noexcept { return controllerStickThreshold_; }
    /// @brief トリガー入力をデバイス切り替えとみなす押し込み量（0.0～1.0）を設定
    void SetControllerTriggerThreshold(float threshold) noexcept;
    /// @brief トリガー入力をデバイス切り替えとみなす押し込み量（0.0～1.0）を取得
    float GetControllerTriggerThreshold() const noexcept { return controllerTriggerThreshold_; }
    /// @brief 入力デバイス判定のしきい値を EngineSettings.json の値（input セクション）へ戻す
    /// @details シーンコンポーネント破棄時（シーン切り替え・Play停止・エディターでの再読み込み）に
    ///          `Scene` から呼ばれ、シーンごとの上書きやスクリプトからの変更を取り消す
    void ResetDeviceThresholds();

#if defined(USE_IMGUI)
    /// @brief 入力状態のデバッグ表示（ImGui ウィンドウ）
    void ShowImGui();
#endif

private:
    /// @brief 各デバイスの入力状況から現在の入力デバイスを更新する
    void UpdateActiveDevice_();
    /// @brief このフレームでキーボード＋マウスに有効な入力があったかを判定する
    bool DetectKeyboardMouseActivity_();
    /// @brief このフレームで有効な入力があったコントローラーのインデックスを取得する（なければ -1）
    /// @details 現在のアクティブなコントローラーに入力があればそれを優先する
    int DetectControllerActivity_() const;

    std::unique_ptr<Keyboard> keyboard_;
    std::unique_ptr<Mouse> mouse_;
    std::unique_ptr<Controller> controller_;

    InputDeviceType activeDevice_ = InputDeviceType::KeyboardMouse;
    bool activeDeviceChanged_ = false;
    int activeControllerIndex_ = -1;

    // 初期値はコンストラクタで EngineSettings から設定する
    float mouseMoveThreshold_ = 0.0f;
    float controllerStickThreshold_ = 0.0f;
    float controllerTriggerThreshold_ = 0.0f;

    // 小さな揺れを除外するため、一連のマウス移動の累積量で判定する
    float mouseMoveAccumulated_ = 0.0f;
    std::chrono::steady_clock::time_point lastMouseMoveTime_{};
};

} // namespace KashipanEngine
