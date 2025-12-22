#if defined(USE_IMGUI)

#include "ImGuiManager.h"

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
    // DX12SwapChain 側の既定が DXGI_FORMAT_B8G8R8A8_UNORM のため合わせる
    return DXGI_FORMAT_B8G8R8A8_UNORM;
}

// ImGui DX12 backend (docking版) が要求する Legacy single SRV descriptor を確保
// (ImGui_ImplDX12_InitInfo::LegacySingleSrvCpuDescriptor/GpuDescriptor)
std::unique_ptr<DescriptorHandleInfo> sImGuiLegacySrv;
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

    // メインウィンドウを取得（EngineSettings の initialWindowTitle で解決）
    // GameEngine の生成順では ImGuiManager が Window 作成より先なので、ここではまだ得られない場合がある。
    // BeginFrame 側で遅延初期化（バックエンド初期化）を行う。

    isInitialized_ = true;
}

void ImGuiManager::ShutdownInternal() {
    if (!isInitialized_) return;

    // バックエンドが初期化されている場合のみ shutdown
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();

    ImGui::DestroyContext();

    // ImGui 用に確保した legacy SRV を開放
    sImGuiLegacySrv.reset();

    isInitialized_ = false;
}

static HWND ResolveMainHwnd() {
    const auto& title = GetEngineSettings().window.initialWindowTitle;
    auto windows = Window::GetWindows(title);
    if (!windows.empty() && windows.front()) return windows.front()->GetWindowHandle();

    // エンジン側でタイトルを変えている可能性に備え、既知の "Main Window" も見る
    windows = Window::GetWindows("Main Window");
    if (!windows.empty() && windows.front()) return windows.front()->GetWindowHandle();

    return nullptr;
}

void ImGuiManager::BeginFrame(Passkey<GameEngine>) {
    if (!isInitialized_) return;

    // バックエンド初期化はウィンドウ生成後に行う
    if (!isBackendInitialized_) {
        mainHwnd_ = ResolveMainHwnd();
        if (!mainHwnd_) return;

        // Win32 backend
        if (!ImGui_ImplWin32_Init(mainHwnd_)) {
            return;
        }

        // DX12 backend
        auto* device = directXCommon_->GetDeviceForImGui();
        auto* srvHeap = directXCommon_->GetSRVHeapForImGui();
        if (!device || !srvHeap) {
            return;
        }

        // Docking 版 imgui_impl_dx12 は legacy single SRV デスクリプタを要求する
        if (!sImGuiLegacySrv) {
            sImGuiLegacySrv = srvHeap->AllocateDescriptorHandle();
        }

        ImGui_ImplDX12_InitInfo info{};
        info.Device = device;
        info.CommandQueue = directXCommon_->GetCommandQueueForImGui();
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

    // デモ: docking + viewport 前提のウィンドウ
    ImGui::Begin("KashipanEngine Debug");
    ImGui::Text("ImGui is running.");
    ImGui::Text("Docking: %s", (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable) ? "ON" : "OFF");
    ImGui::Text("Viewports: %s", (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) ? "ON" : "OFF");
    ImGui::End();
}

void ImGuiManager::Render(Passkey<GameEngine>) {
    if (!isInitialized_) return;
    if (!isBackendInitialized_) return;

    ImGui::Render();

    // メインウィンドウのコマンドリストへ描画
    auto hwnd = ResolveMainHwnd();
    if (hwnd) {
        if (auto* cmd = directXCommon_->GetRecordedCommandListForImGui(hwnd)) {
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd);
        }
    }

    // マルチビューポート: プラットフォームウィンドウを更新・描画
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

} // namespace KashipanEngine

#endif // defined(USE_IMGUI)
