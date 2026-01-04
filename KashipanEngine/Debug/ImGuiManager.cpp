#if defined(USE_IMGUI)

#include "ImGuiManager.h"

#include "Assets/TextureManager.h"
#include "Assets/ModelManager.h"
#include "Assets/AudioManager.h"
#include "Core/WindowsAPI.h"
#include "Core/DirectXCommon.h"
#include "Core/Window.h"
#include "EngineSettings.h"
#include "Utilities/Conversion/ConvertString.h"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>

#include <d3d12.h>

namespace KashipanEngine {

namespace {
DXGI_FORMAT ToDxgiFormat_WindowsSwapChain() {
    return DXGI_FORMAT_B8G8R8A8_UNORM;
}

std::unique_ptr<DescriptorHandleInfo> sImGuiLegacySrv;

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
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // 現在の言語環境に合わせてフォントを設定
    {
        // フォントの大きさをDPIに基づいて設定
        auto dpi = GetDpiForSystem();
        float fontSizeDefault = 16.0f;
        float fontSize = fontSizeDefault * (static_cast<float>(dpi) / 96.0f);
        std::string fontPath = GetCurrentLanguageFontPath();
        if (!fontPath.empty()) {
            io.Fonts->AddFontFromFileTTF(fontPath.c_str(), fontSize, nullptr, io.Fonts->GetGlyphRangesJapanese());
        } else {
            io.Fonts->AddFontDefault();
        }
    }

    isInitialized_ = true;
}

void ImGuiManager::ShutdownInternal() {
    if (!isInitialized_) return;

    if (isBackendInitialized_) {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        isBackendInitialized_ = false;
    }

    ImGui::DestroyContext();
    sImGuiLegacySrv.reset();

    isInitialized_ = false;
}

void ImGuiManager::BeginFrame(Passkey<GameEngine>) {
    if (!isInitialized_) return;

    // バックエンド初期化はウィンドウ生成後に行う
    if (!isBackendInitialized_) {
        mainHwnd_ = Window::GetFirstWindowHwndForImGui({});
        if (!mainHwnd_) return;

        if (!ImGui_ImplWin32_Init(mainHwnd_)) {
            return;
        }

        auto* device = directXCommon_->GetDeviceForImGui({});
        auto* srvHeap = directXCommon_->GetSRVHeapForImGui({});
        if (!device || !srvHeap) {
            return;
        }

        if (!sImGuiLegacySrv) {
            sImGuiLegacySrv = srvHeap->AllocateDescriptorHandle();
        }

        ImGui_ImplDX12_InitInfo info{};
        info.Device = device;
        info.CommandQueue = directXCommon_->GetCommandQueueForImGui({});
        info.NumFramesInFlight = 2;
        info.RTVFormat = ToDxgiFormat_WindowsSwapChain();
        info.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

        info.SrvDescriptorHeap = srvHeap->GetDescriptorHeap();
        info.SrvDescriptorAllocFn = nullptr;
        info.SrvDescriptorFreeFn = nullptr;

        info.LegacySingleSrvCpuDescriptor = sImGuiLegacySrv->cpuHandle;
        info.LegacySingleSrvGpuDescriptor = sImGuiLegacySrv->gpuHandle;

        if (!ImGui_ImplDX12_Init(&info)) {
            return;
        }

        isBackendInitialized_ = true;
    }

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    TextureManager::ShowImGuiLoadedTexturesWindow();
    ModelManager::ShowImGuiLoadedModelsWindow();
    AudioManager::ShowImGuiLoadedSoundsWindow();
    AudioManager::ShowImGuiPlayingSoundsWindow();
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
                if (!mainHwnd_) mainHwnd_ = hwnd;

                if (auto* cmd = directXCommon_->GetRecordedCommandListForImGui({}, hwnd)) {
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
