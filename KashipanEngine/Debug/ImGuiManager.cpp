#if defined(USE_IMGUI)

#include "ImGuiManager.h"

#include "Assets/TextureManager.h"
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

ImTextureID ToImGuiTextureIdFromGpuPtr(UINT64 gpuPtr) {
    return (ImTextureID)(uintptr_t)gpuPtr;
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

    ImGui::Begin("KashipanEngine デバッグ用ウィンドウ");
    ImGui::Text("ImGui 実行中。");
    ImGui::Text("ImGuiドッキング: %s", (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable) ? "有効化中" : "無効化中");
    ImGui::Text("ImGuiマルチビューポート: %s", (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) ? "有効化中" : "無効化中");
    ImGui::End();

    // TextureManager 読み込みテスト用ウィンドウ
    {
        ImGui::Begin("TextureManager - Loaded Textures");

        const auto entries = TextureManager::GetImGuiTextureListEntries();
        ImGui::Text("Loaded Textures: %d", static_cast<int>(entries.size()));

        static ImGuiTextFilter filter;
        filter.Draw("Filter");

        // 選択中テクスチャ（拡大表示用）
        static TextureManager::TextureListEntry sSelectedTexture{};
        static bool sShowTextureViewer = false;

        ImGui::Separator();

        if (ImGui::BeginTable("##TextureList", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 300))) {
            ImGui::TableSetupColumn("Handle", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("FileName");
            ImGui::TableSetupColumn("AssetPath");
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 90);
            ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableHeadersRow();

            for (const auto& e : entries) {
                if (filter.IsActive()) {
                    if (!filter.PassFilter(e.fileName.c_str()) && !filter.PassFilter(e.assetPath.c_str())) {
                        continue;
                    }
                }

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%u", e.handle);

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(e.fileName.c_str());

                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(e.assetPath.c_str());

                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%ux%u", e.width, e.height);

                ImGui::TableSetColumnIndex(4);
                if (e.srvGpuPtr != 0) {
                    ImGui::PushID(static_cast<int>(e.handle));
                    const auto texId = ToImGuiTextureIdFromGpuPtr(e.srvGpuPtr);
                    if (ImGui::ImageButton("##Preview", texId, ImVec2(64, 64))) {
                        sSelectedTexture = e;
                        sShowTextureViewer = true;
                    }
                    ImGui::PopID();
                } else {
                    ImGui::TextUnformatted("-");
                }
            }

            ImGui::EndTable();
        }

        ImGui::End();

        // 選択中テクスチャの拡大表示ウィンドウ
        if (sShowTextureViewer) {
            if (ImGui::Begin("Texture Viewer", &sShowTextureViewer)) {
                if (sSelectedTexture.srvGpuPtr != 0) {
                    ImGui::Text("Handle: %u", sSelectedTexture.handle);
                    ImGui::TextUnformatted(sSelectedTexture.assetPath.c_str());
                    ImGui::Separator();

                    // ウィンドウ内で収まるようにスケーリング
                    ImVec2 avail = ImGui::GetContentRegionAvail();
                    const float w = static_cast<float>(sSelectedTexture.width);
                    const float h = static_cast<float>(sSelectedTexture.height);
                    ImVec2 drawSize = avail;
                    if (w > 0.0f && h > 0.0f) {
                        const float sx = avail.x / w;
                        const float sy = avail.y / h;
                        const float s = (sx < sy) ? sx : sy;
                        drawSize = ImVec2(w * s, h * s);
                    }

                    ImGui::Image(ToImGuiTextureIdFromGpuPtr(sSelectedTexture.srvGpuPtr), drawSize);
                } else {
                    ImGui::TextUnformatted("No texture selected.");
                }
            }
            ImGui::End();
        }
    }

    ImGui::ShowDemoWindow();
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
