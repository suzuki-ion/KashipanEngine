#if defined(USE_IMGUI)

#include "ImGuiManager.h"

#include "Core/WindowsAPI.h"
#include "Core/DirectXCommon.h"
#include "Core/Window.h"
#include "EngineSettings.h"
#include "Scene/Editor/EditorSettings.h"
#include "Utilities/Conversion/ConvertString.h"

#include <algorithm>

#include <imgui.h>
#include <ImGuizmo.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>

#include <d3d12.h>
#include <unordered_map>

namespace KashipanEngine {

namespace {
DXGI_FORMAT ToDxgiFormat_WindowsSwapChain() {
    return DXGI_FORMAT_B8G8R8A8_UNORM;
}

// ImGuiの内部テクスチャ（フォントアトラス等）用SRVディスクリプタの割り当て先ヒープ。
// Dear ImGui 1.92以降はフォントのグリフを実際に描画された時点で遅延読み込みするため、
// 未表示だった文字（例: スクロールで初めて見えたログの日本語文字）が新たに描画される度に
// アトラステクスチャが再構築され、複数のテクスチャが同時に生存しうる。
// そのため単一ディスクリプタ（レガシーモード）ではなく、必要な数だけ動的に確保できる
// ようにする必要がある（ImGuiSrvDescriptorAllocFn/FreeFn参照）
SRVHeap *sImGuiSrvHeap = nullptr;
// GPUディスクリプタハンドルのポインタ値をキーに、確保済みディスクリプタを保持する
// （キーで解放対象を特定する。unique_ptrの破棄でDescriptorHeapBaseへ返却される）
std::unordered_map<UINT64, std::unique_ptr<DescriptorHandleInfo>> sImGuiTextureDescriptors;

void ImGuiSrvDescriptorAllocFn(ImGui_ImplDX12_InitInfo *, D3D12_CPU_DESCRIPTOR_HANDLE *outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE *outGpuHandle) {
    auto handle = sImGuiSrvHeap->AllocateDescriptorHandle();
    *outCpuHandle = handle->cpuHandle;
    *outGpuHandle = handle->gpuHandle;
    sImGuiTextureDescriptors.emplace(handle->gpuHandle.ptr, std::move(handle));
}

void ImGuiSrvDescriptorFreeFn(ImGui_ImplDX12_InitInfo *, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle) {
    sImGuiTextureDescriptors.erase(gpuHandle.ptr);
}

HWND PlatformHwndFromViewport(ImGuiViewport* vp) {
    if (!vp) return nullptr;
    return (HWND)vp->PlatformHandleRaw;
}

} // namespace

ImGuiManager::ImGuiManager(Passkey<GameEngine>, WindowsAPI* windowsAPI, DirectXCommon* directXCommon)
    : windowsAPI_(windowsAPI), directXCommon_(directXCommon) {
    InitializeInternal();
}

ImGuiManager::~ImGuiManager() {
    ShutdownInternal();
}

void ImGuiManager::InitializeInternal() {
    if (isInitialized_) return;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    if (!(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)) {
        // マルチビューポートを使用しない場合はImGui用のウィンドウを作成
        Window::CreateNormal(
            "ImGui Window",
            1280, 720
        );
    }

    // エディターUI設定（EditorSettings）を初期適用する（フォント・スケール・配色）
    // ImGuiManagerがEditorSettingsを直接読みに行く（SceneEditor側からの受け渡しはしない）
    appliedFontPath_ = EditorSettings::GetString("editorUI.fontPath", "");
    RebuildFontAtlas(appliedFontPath_);
    appliedUIScale_ = EditorSettings::GetFloat("editorUI.uiScale", 1.0f);
    appliedColors_ = EditorSettings::GetJSON("editorUI.colors", JSON());
    ReapplyStyle(appliedUIScale_, appliedColors_);
    io.FontGlobalScale = std::max(0.1f, EditorSettings::GetFloat("editorUI.fontScale", 1.0f));

    isInitialized_ = true;
}

void ImGuiManager::RebuildFontAtlas(const std::string &fontPath) {
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->Clear();
    io.FontDefault = nullptr;

    // フォントの大きさをDPIに基づいて設定
    auto dpi = GetDpiForSystem();
    const float fontSizeDefault = 16.0f;
    const float fontSize = fontSizeDefault * (static_cast<float>(dpi) / 96.0f);

    // 空文字の場合は現在の言語のデフォルトフォントを使う
    const std::string resolvedPath = fontPath.empty() ? GetCurrentLanguageFontPath() : fontPath;
    ImFont *font = nullptr;
    if (!resolvedPath.empty()) {
        font = io.Fonts->AddFontFromFileTTF(resolvedPath.c_str(), fontSize, nullptr, io.Fonts->GetGlyphRangesJapanese());
    }
    if (!font) {
        // 指定フォントの読み込みに失敗した場合はImGui組み込みのデフォルトフォントにフォールバックする
        font = io.Fonts->AddFontDefault();
    }
    io.FontDefault = font;
}

void ImGuiManager::ReapplyStyle(float uiScale, const JSON &colorsJson) {
    ImGuiStyle &style = ImGui::GetStyle();
    // サイズ・配色を素の状態（スケール1.0相当）へ戻してから組み立て直す。
    // ScaleAllSizesは呼ぶたびに現在値へ倍率をかける累積処理のため、都度リセットしないと
    // スケールを変更するたびに値がどんどん膨れ上がってしまう
    style = ImGuiStyle();

    if (colorsJson.is_array() && colorsJson.size() == ImGuiCol_COUNT) {
        for (int i = 0; i < ImGuiCol_COUNT; ++i) {
            const auto &c = colorsJson[i];
            if (c.is_array() && c.size() == 4) {
                style.Colors[i] = ImVec4(c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>());
            }
        }
    } else {
        ImGui::StyleColorsDark(&style);
    }

    const ImGuiIO &io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    style.ScaleAllSizes(std::max(0.1f, uiScale));
}

void ImGuiManager::ApplyEditorPreferencesIfChanged() {
    // 文字サイズ（FontGlobalScale）はアトラスの作り直しが不要なため、変更検知なしで毎フレーム同期する
    ImGuiIO &io = ImGui::GetIO();
    io.FontGlobalScale = std::max(0.1f, EditorSettings::GetFloat("editorUI.fontScale", 1.0f));

    // フォント差し替えはアトラスの作り直しが必要なため、パスが変わった場合のみ行う
    const std::string fontPath = EditorSettings::GetString("editorUI.fontPath", "");
    if (fontPath != appliedFontPath_) {
        RebuildFontAtlas(fontPath);
        appliedFontPath_ = fontPath;
    }

    // 全体スケール・配色は変化した場合のみスタイルを再構築する
    const float uiScale = EditorSettings::GetFloat("editorUI.uiScale", 1.0f);
    const JSON colors = EditorSettings::GetJSON("editorUI.colors", JSON());
    if (uiScale != appliedUIScale_ || colors != appliedColors_) {
        ReapplyStyle(uiScale, colors);
        appliedUIScale_ = uiScale;
        appliedColors_ = colors;
    }
}

void ImGuiManager::ShutdownInternal() {
    if (!isInitialized_) return;

    if (isBackendInitialized_) {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        isBackendInitialized_ = false;
    }

    ImGui::DestroyContext();
    // ImGui_ImplDX12_Shutdown() の中でImGuiが保持する全テクスチャの解放（SrvDescriptorFreeFn呼び出し）が
    // 行われるため通常は空になっているはずだが、念のため残りがあればここでヒープへ返却しておく
    sImGuiTextureDescriptors.clear();
    sImGuiSrvHeap = nullptr;

    SetMainHwnd(nullptr);
    isInitialized_ = false;
}

void ImGuiManager::BeginFrame(Passkey<GameEngine>) {
    if (!isInitialized_) return;

    // エディターUI設定の変更を毎フレーム確認して反映する（フォントアトラスの作り直しを伴いうるため、
    // NewFrame()より前に行う）
    ApplyEditorPreferencesIfChanged();

    // バックエンド初期化はウィンドウ生成後に行う
    if (!isBackendInitialized_) {
        ImGuiIO &io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            SetMainHwnd(Window::GetFirstWindowHwndForImGui({}));
        } else {
            Window *window = Window::GetWindow("ImGui Window");
            if (!window) return;
            SetMainHwnd(window->GetWindowHandle());
        }
        if (!mainHwnd_) return;

        if (!ImGui_ImplWin32_Init(mainHwnd_)) {
            return;
        }

        auto* device = directXCommon_->GetDeviceForImGui({});
        auto* srvHeap = directXCommon_->GetSRVHeapForImGui({});
        if (!device || !srvHeap) {
            return;
        }
        sImGuiSrvHeap = srvHeap;

        ImGui_ImplDX12_InitInfo info{};
        info.Device = device;
        info.CommandQueue = directXCommon_->GetCommandQueueForImGui({});
        info.NumFramesInFlight = 2;
        info.RTVFormat = ToDxgiFormat_WindowsSwapChain();
        info.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

        info.SrvDescriptorHeap = srvHeap->GetDescriptorHeap();
        // フォントアトラスの遅延グリフ読み込みで複数テクスチャが同時に生存しうるため、
        // 単一ディスクリプタのレガシーモードではなく、共有SRVヒープから動的に確保する
        info.SrvDescriptorAllocFn = &ImGuiSrvDescriptorAllocFn;
        info.SrvDescriptorFreeFn = &ImGuiSrvDescriptorFreeFn;

        if (!ImGui_ImplDX12_Init(&info)) {
            return;
        }

        isBackendInitialized_ = true;
    }

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();

    ImGuiIO &io = ImGui::GetIO();
    if (!(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)) {
        // マルチビューポートを使用しない場合ImGui用ウィンドウ全体に対してドッキングを有効にする
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
    }
}

void ImGuiManager::Render(Passkey<GameEngine>) {
    if (!isInitialized_) return;
    if (!isBackendInitialized_) return;

    ImGui::Render();

    // メイン viewport（= 現在のエンジンの描画ターゲット）に対してのみ描画する
    {
        ImGuiViewport* mainVp = ImGui::GetMainViewport();
        if (mainVp) {
            HWND hwnd = PlatformHwndFromViewport(mainVp);
            if (hwnd) {
                // mainHwnd_ が未解決の場合は補完
                if (!mainHwnd_) SetMainHwnd(hwnd);

                if (auto* cmd = directXCommon_->GetRecordedCommandListForImGui({}, hwnd)) {
                    Window::GetWindow(mainHwnd_)->BeginDraw();
                    ImGui_ImplDX12_RenderDrawData(mainVp->DrawData, cmd);
                }
            }
        }
    }

    // マルチビューポートは ImGui backend に任せる
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

} // namespace KashipanEngine

#endif // defined(USE_IMGUI)
