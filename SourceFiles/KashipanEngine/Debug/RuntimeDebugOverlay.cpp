#if defined(RELEASE_BUILD)
#include "RuntimeDebugOverlay.h"
#include "Debug/Profiler.h"
#include "Core/DirectXCommon.h"
#include "Core/Window.h"
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <unordered_map>

namespace KashipanEngine {
struct RuntimeDebugOverlay::Impl {
    DirectXCommon *directX;
    ImGuiContext *context = nullptr;
    SRVHeap *heap = nullptr;
    bool initialized = false;
    std::unordered_map<UINT64, std::unique_ptr<DescriptorHandleInfo>> handles;
    static void Allocate(ImGui_ImplDX12_InitInfo *info, D3D12_CPU_DESCRIPTOR_HANDLE *cpu, D3D12_GPU_DESCRIPTOR_HANDLE *gpu) {
        auto &self = *static_cast<Impl *>(info->UserData);
        auto handle = self.heap->AllocateDescriptorHandle();
        *cpu = handle->cpuHandle;
        *gpu = handle->gpuHandle;
        self.handles.emplace(gpu->ptr, std::move(handle));
    }
    static void Free(ImGui_ImplDX12_InitInfo *info, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE gpu) {
        static_cast<Impl *>(info->UserData)->handles.erase(gpu.ptr);
    }
};
RuntimeDebugOverlay::RuntimeDebugOverlay(Passkey<GameEngine>, DirectXCommon *directX)
    : impl_(std::make_unique<Impl>()) { impl_->directX = directX; }
RuntimeDebugOverlay::~RuntimeDebugOverlay() {
    if (!impl_->context) return;
    ImGui::SetCurrentContext(impl_->context);
    if (impl_->initialized) ImGui_ImplDX12_Shutdown();
    ImGui::DestroyContext(impl_->context);
}
void RuntimeDebugOverlay::Draw(Window *window, bool showFps, bool showProfiling) {
    if (!window || window->IsPendingDestroy() || !window->IsRenderTargetAvailable()) return;
    if (!impl_->context) {
        IMGUI_CHECKVERSION();
        impl_->context = ImGui::CreateContext();
        ImGui::GetIO().IniFilename = nullptr;
        ImGui::GetIO().LogFilename = nullptr;
    }
    ImGui::SetCurrentContext(impl_->context);
    if (!impl_->initialized) {
        impl_->heap = impl_->directX->GetSRVHeapForDebugOverlay({});
        ImGui_ImplDX12_InitInfo info{};
        info.UserData = impl_.get();
        info.Device = impl_->directX->GetDeviceForDebugOverlay({});
        info.CommandQueue = impl_->directX->GetCommandQueueForDebugOverlay({});
        info.NumFramesInFlight = 2;
        info.RTVFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
        info.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        info.SrvDescriptorHeap = impl_->heap->GetDescriptorHeap();
        info.SrvDescriptorAllocFn = &Impl::Allocate;
        info.SrvDescriptorFreeFn = &Impl::Free;
        if (!ImGui_ImplDX12_Init(&info)) return;
        impl_->initialized = true;
    }
    auto snapshot = Profiler::GetInstance().GetSnapshot();
    std::stable_sort(snapshot.results.begin(), snapshot.results.end(), [](const auto &a, const auto &b) {
        return a.averageMs > b.averageMs;
    });
    auto &io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(window->GetRenderTargetWidth()), static_cast<float>(window->GetRenderTargetHeight()));
    io.DeltaTime = snapshot.fps > 0 ? static_cast<float>(1.0 / snapshot.fps) : 1.0f / 60.0f;
    ImGui_ImplDX12_NewFrame();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.8f);
    ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(std::max(1.0f, io.DisplaySize.x - 20), std::max(1.0f, io.DisplaySize.y - 20)));
    if (ImGui::Begin("Runtime diagnostics", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing)) {
        if (showFps) ImGui::Text("FPS %.1f | CPU frame %.3f ms", snapshot.fps, snapshot.frameMs);
        if (showProfiling) {
            ImGui::Text("CPU inclusive time (current / average / peak ms), %zu frames", snapshot.frames);
            const auto rows = static_cast<std::size_t>(std::max(0.0f,
                (io.DisplaySize.y - 100.0f) / ImGui::GetTextLineHeightWithSpacing()));
            const auto count = std::min(rows, snapshot.results.size());
            for (std::size_t i = 0; i < count; ++i) {
                const auto &result = snapshot.results[i];
                ImGui::Text("%s: %.3f / %.3f / %.3f (%llu calls)", result.name.c_str(), result.milliseconds,
                    result.averageMs, result.peakMs, static_cast<unsigned long long>(result.calls));
            }
            if (count < snapshot.results.size()) ImGui::Text("Showing %zu / %zu processes, highest average first", count, snapshot.results.size());
        }
    }
    ImGui::End();
    ImGui::Render();
    window->BeginDraw();
    if (auto *commandList = window->GetCommandList()) ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}
}
#endif
