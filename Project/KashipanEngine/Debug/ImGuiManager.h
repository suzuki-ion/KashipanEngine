#pragma once
#if defined(USE_IMGUI)

#include <Windows.h>
#include <string>
#include "Utilities/FileIO/JSON.h"
#include "Utilities/Passkeys.h"

struct ID3D12DescriptorHeap;
struct ID3D12GraphicsCommandList;

namespace KashipanEngine {

class GameEngine;
class WindowsAPI;
class DirectXCommon;

/// @brief ImGui 管理クラス
class ImGuiManager final {
public:
    ImGuiManager(Passkey<GameEngine>, WindowsAPI* windowsAPI, DirectXCommon* directXCommon);
    ~ImGuiManager();

    ImGuiManager(const ImGuiManager&) = delete;
    ImGuiManager& operator=(const ImGuiManager&) = delete;
    ImGuiManager(ImGuiManager&&) = delete;
    ImGuiManager& operator=(ImGuiManager&&) = delete;

    void BeginFrame(Passkey<GameEngine>);
    void Render(Passkey<GameEngine>);

    bool IsInitialized() const noexcept { return isInitialized_; }

    /// @brief ImGui のメインウィンドウハンドルを取得（未初期化の場合は nullptr）
    /// @details ウィンドウプロシージャが ImGui へイベントを転送するかの判定に使う。
    ///          他ウィンドウのマウス座標が ImGui へ流れないようにするため、
    ///          このハンドルのウィンドウ宛のメッセージのみ転送する。
    static HWND GetMainWindowHwnd() noexcept { return sMainHwnd_; }

private:
    void InitializeInternal();
    void ShutdownInternal();

    void SetMainHwnd(HWND hwnd) noexcept {
        mainHwnd_ = hwnd;
        sMainHwnd_ = hwnd;
    }

    //==================================================
    // エディターUI設定（EditorSettings直読み）
    //==================================================

    /// @brief EditorSettingsを毎フレーム確認し、変更があった項目だけ適用する
    void ApplyEditorPreferencesIfChanged();
    /// @brief フォントアトラスを作り直す（fontPathが空の場合は現在の言語のデフォルトフォントを使う）
    void RebuildFontAtlas(const std::string &fontPath);
    /// @brief スタイル（配色・全体スケール）を素の状態から再構築する
    void ReapplyStyle(float uiScale, const JSON &colorsJson);

    static inline HWND sMainHwnd_ = nullptr;

    WindowsAPI* windowsAPI_ = nullptr;
    DirectXCommon* directXCommon_ = nullptr;

    HWND mainHwnd_ = nullptr;

    bool isBackendInitialized_ = false;
    bool isInitialized_ = false;

    // 直近で適用済みの値（EditorSettingsとの差分検知用）
    std::string appliedFontPath_;
    float appliedUIScale_ = 1.0f;
    JSON appliedColors_;
};

} // namespace KashipanEngine

#endif
