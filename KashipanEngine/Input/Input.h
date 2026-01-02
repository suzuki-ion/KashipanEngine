#pragma once

#include <memory>
#include <windows.h>

namespace KashipanEngine {

class Keyboard;
class Mouse;
class Controller;

class Input {
public:
    Input();
    ~Input();

    Input(const Input&) = delete;
    Input(Input&&) = delete;
    Input& operator=(const Input&) = delete;
    Input& operator=(Input&&) = delete;

    void Initialize(HINSTANCE hInstance, HWND hwnd);
    void Finalize();
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

#if defined(USE_IMGUI)
    /// @brief 入力状態のデバッグ表示（ImGui ウィンドウ）
    void ShowImGui();
#endif

private:
    std::unique_ptr<Keyboard> keyboard_;
    std::unique_ptr<Mouse> mouse_;
    std::unique_ptr<Controller> controller_;
};

} // namespace KashipanEngine
