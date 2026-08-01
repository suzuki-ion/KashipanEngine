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

    /// @brief メインDockSpaceのノードを削除し、次フレームの開始時に既定のドッキング配置を
    ///        再構築させる（エディター環境設定の「レイアウトを既定に戻す」から呼ばれる）
    static void ResetDockLayoutToDefault();

    /// @brief 次フレーム開始時（ImGui::NewFrame()の直前）に適用するini設定（ドッキングレイアウト
    ///        プリセット）を予約する
    /// @details ImGui::LoadIniSettingsFromMemory()は本来NewFrame()呼び出しより前に行うべき操作。
    ///          フレームの途中（ボタン押下時など、既に多数のウィンドウがBeginされた後）に直接
    ///          呼ぶとドッキングツリーの再構築タイミングがずれ、その時点より後の描画順で
    ///          Beginされるウィンドウ（例: 環境設定パネルより後に描画される翻訳キー設定ウィンドウや、
    ///          SceneEditor::ShowImGui()全体より後に描画されるScreenBufferObjectのビューアなど）
    ///          だけドッキングが解除されてしまう。次フレームの先頭で適用することでこれを避ける
    static void RequestLoadIniSettings(std::string iniText);

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
    /// @brief スタイル（配色・角丸/余白等の詳細設定・全体スケール）を素の状態から再構築する
    void ReapplyStyle(float uiScale, const JSON &colorsJson, const JSON &styleJson);

    static inline HWND sMainHwnd_ = nullptr;
    /// @brief RequestLoadIniSettings()で予約された、次フレーム開始時に適用するini設定
    static inline std::string sPendingIniSettings_;
    static inline bool sHasPendingIniSettings_ = false;

    WindowsAPI* windowsAPI_ = nullptr;
    DirectXCommon* directXCommon_ = nullptr;

    HWND mainHwnd_ = nullptr;

    bool isBackendInitialized_ = false;
    bool isInitialized_ = false;

    // 直近で適用済みの値（EditorSettingsとの差分検知用）
    std::string appliedFontPath_;
    std::string appliedLanguage_;
    float appliedUIScale_ = 1.0f;
    JSON appliedColors_;
    JSON appliedStyle_;
};

} // namespace KashipanEngine

#endif
