#include "DirectXCommon.h"
#include "EngineSettings.h"
#include "Graphics/Resources.h"
#include "Graphics/ScreenBuffer.h"
#include <vector>
#include <unordered_map>
#include <functional>
#include <stdexcept>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace KashipanEngine {

namespace {
// 遅延初期化スワップチェーンスロット管理
std::vector<std::unique_ptr<DX12SwapChain>> sSwapChains;               // インデックスアクセス用
std::unordered_map<HWND, size_t> sHwndToSwapChainIndex;                // HWND -> インデックス
std::vector<size_t> sFreeSwapChains;                                   // 空きスワップチェーンインデックス
std::vector<HWND> sPendingDestroySwapChains;                           // 破棄指示された HWND
} // namespace

DirectXCommon::DirectXCommon(Passkey<GameEngine>, bool enableDebugLayer) {
    LogScope scope;
    Log(Translation("engine.directx.initialize.start"), LogSeverity::Debug);

#if defined(DEBUG_BUILD) || defined(DEVELOPMENT_BUILD)
    Log(enableDebugLayer ?
        Translation("engine.directx.debuglayer.enabled") :
        Translation("engine.directx.debuglayer.disabled"),
        LogSeverity::Debug);
    Microsoft::WRL::ComPtr<ID3D12Debug1> debugController;
    if (enableDebugLayer) {
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
            debugController->EnableDebugLayer();
            debugController->SetEnableGPUBasedValidation(true);
        }
    }
#else
    Log(Translation("engine.directx.debuglayer.disabled.release"), LogSeverity::Debug);
    static_cast<void>(enableDebugLayer);
#endif

    dx12DXGIs_ = std::make_unique<DX12DXGIs>(Passkey<DirectXCommon>{});
    dx12Device_ = std::make_unique<DX12Device>(Passkey<DirectXCommon>{}, dx12DXGIs_->GetDXGIAdapter());
    dx12CommandQueue_ = std::make_unique<DX12CommandQueue>(Passkey<DirectXCommon>{}, dx12Device_->GetDevice(), D3D12_COMMAND_LIST_TYPE_DIRECT);
    dx12Fence_ = std::make_unique<DX12Fence>(Passkey<DirectXCommon>{}, dx12Device_->GetDevice());

    IGraphicsResource::SetDevice({}, dx12Device_->GetDevice());

    auto &settings = GetEngineSettings().rendering;
    RenderTargetResource::SetDefaultClearColor(Passkey<DirectXCommon>{}, settings.defaultClearColor);

    RTVHeap_ = std::make_unique<RTVHeap>(Passkey<DirectXCommon>{}, dx12Device_->GetDevice(), settings.rtvDescriptorHeapSize);
    DSVHeap_ = std::make_unique<DSVHeap>(Passkey<DirectXCommon>{}, dx12Device_->GetDevice(), settings.dsvDescriptorHeapSize);
    SRVHeap_ = std::make_unique<SRVHeap>(Passkey<DirectXCommon>{}, dx12Device_->GetDevice(), settings.srvDescriptorHeapSize);
    SamplerHeap_ = std::make_unique<SamplerHeap>(Passkey<DirectXCommon>{}, dx12Device_->GetDevice(), settings.srvDescriptorHeapSize);

    // グローバルヒープポインタ設定
    IGraphicsResource::SetDescriptorHeaps({},
        RTVHeap_.get(), DSVHeap_.get(), SRVHeap_.get(), SamplerHeap_.get());

    DX12SwapChain::Initialize(Passkey<DirectXCommon>{}, this,
        dx12Device_->GetDevice(),
        dx12DXGIs_->GetDXGIFactory(),
        dx12CommandQueue_->GetCommandQueue(),
        RTVHeap_.get(),
        DSVHeap_.get(),
        SRVHeap_.get(),
        SamplerHeap_.get()
    );

    // スワップチェーンスロット事前確保 (遅延初期化用空インスタンス生成)
    size_t maxWindows = GetEngineSettings().limits.maxWindows;
    sSwapChains.resize(maxWindows);
    sFreeSwapChains.reserve(maxWindows);

    swapChainCommandObjects_.clear();
    swapChainCommandObjects_.resize(maxWindows);

    for (size_t i = 0; i < maxWindows; ++i) {
        sSwapChains[i] = std::make_unique<DX12SwapChain>(Passkey<DirectXCommon>{});
        sFreeSwapChains.push_back(i);
    }

    Log(Translation("engine.directx.initialize.end"), LogSeverity::Debug);
}

DirectXCommon::~DirectXCommon() {
    LogScope scope;
    Log(Translation("engine.directx.finalize.start"), LogSeverity::Debug);

    if (dx12Fence_ && dx12CommandQueue_) {
        dx12Fence_->Signal(Passkey<DirectXCommon>{}, dx12CommandQueue_->GetCommandQueue());
        dx12Fence_->Wait(Passkey<DirectXCommon>{});
    }

    for (auto &sc : sSwapChains) {
        sc.reset();
    }
    sSwapChains.clear();
    sHwndToSwapChainIndex.clear();
    sFreeSwapChains.clear();
    sPendingDestroySwapChains.clear();

    swapChainCommandObjects_.clear();

    screenBufferCommandObjects_.clear();
    freeScreenBufferCommandObjectSlots_.clear();

    shadowMapBufferCommandObjects_.clear();
    freeShadowMapBufferCommandObjectSlots_.clear();

    IGraphicsResource::ClearAllResources({});
    SamplerHeap_.reset();
    SRVHeap_.reset();
    DSVHeap_.reset();
    RTVHeap_.reset();
    dx12Fence_.reset();
    dx12CommandQueue_.reset();
    dx12Device_.reset();
    dx12DXGIs_.reset();
    Log(Translation("engine.directx.finalize.end"), LogSeverity::Debug);
}

void DirectXCommon::BeginDraw(Passkey<GameEngine>) {
}

void DirectXCommon::EndDraw(Passkey<GameEngine>) {
    DestroyPendingSwapChains();
    ExecuteCommand();
}

DX12SwapChain *DirectXCommon::CreateSwapChain(Passkey<Window>, SwapChainType swapChainType, HWND hwnd, int32_t width, int32_t height, int32_t bufferCount) {
    LogScope scope;
    auto it = sHwndToSwapChainIndex.find(hwnd);
    if (it != sHwndToSwapChainIndex.end()) {
        Log(Translation("engine.directx.swapchain.already.exists"), LogSeverity::Warning);
        return sSwapChains[it->second].get();
    }
    if (sFreeSwapChains.empty()) {
        Log(Translation("engine.directx.swapchain.no.free.slot"), LogSeverity::Error);
        return nullptr;
    }

    size_t index = sFreeSwapChains.back();
    sFreeSwapChains.pop_back();

    // bufferCount を反映した新しい空スロットへ差し替え
    sSwapChains[index] = std::make_unique<DX12SwapChain>(Passkey<DirectXCommon>{}, bufferCount);
    DX12SwapChain *sc = sSwapChains[index].get();

    // Command objects (DirectXCommon owns)
    auto &cmd = swapChainCommandObjects_[index];
    cmd.commandAllocators.clear();
    cmd.commandAllocators.resize(static_cast<size_t>(bufferCount));

    for (int i = 0; i < bufferCount; ++i) {
        HRESULT hr = dx12Device_->GetDevice()->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(cmd.commandAllocators[i].ReleaseAndGetAddressOf()));
        if (FAILED(hr)) {
            Log(Translation("engine.directx.swapchain.commandallocator.initialize.failed"), LogSeverity::Critical);
            throw std::runtime_error("Failed to create command allocator for swapchain.");
        }
    }

    if (!cmd.commandList) {
        HRESULT hr = dx12Device_->GetDevice()->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            cmd.commandAllocators[0].Get(),
            nullptr,
            IID_PPV_ARGS(cmd.commandList.ReleaseAndGetAddressOf()));
        if (FAILED(hr)) {
            Log(Translation("engine.directx.swapchain.commandlist.initialize.failed"), LogSeverity::Critical);
            throw std::runtime_error("Failed to create command list for swapchain.");
        }
        cmd.commandList->Close();
    }

    sc->BindCommandObjects(Passkey<DirectXCommon>{}, cmd.commandList.Get(), cmd.commandAllocators);

    sc->AttachWindowAndCreate(Passkey<DirectXCommon>{}, swapChainType, hwnd, width, height);

    bool enableVSync = (swapChainType == SwapChainType::ForComposition) ? true : GetEngineSettings().rendering.defaultEnableVSync;
    sc->SetVSyncEnabled(enableVSync);

    sHwndToSwapChainIndex[hwnd] = index;
    return sc;
}

void DirectXCommon::DestroySwapChainSignal(Passkey<Window>, HWND hwnd) {
    LogScope scope;
    if (sHwndToSwapChainIndex.find(hwnd) == sHwndToSwapChainIndex.end()) {
        Log(Translation("engine.directx.swapchain.notfound"), LogSeverity::Warning);
        return;
    }
    sPendingDestroySwapChains.push_back(hwnd);
}

DX12SwapChain *DirectXCommon::GetSwapChain(Passkey<Renderer>, HWND hwnd) const {
    auto it = sHwndToSwapChainIndex.find(hwnd);
    if (it != sHwndToSwapChainIndex.end()) {
        size_t index = it->second;
        return sSwapChains[index].get();
    }
    return nullptr;
}

ID3D12GraphicsCommandList *DirectXCommon::GetRecordedCommandList(Passkey<Renderer>, HWND hwnd) const {
    auto it = sHwndToSwapChainIndex.find(hwnd);
    if (it != sHwndToSwapChainIndex.end()) {
        size_t index = it->second;
        if (sSwapChains[index]) {
            return sSwapChains[index]->GetRecordedCommandList(Passkey<DirectXCommon>{});
        }
    }
    return nullptr;
}

#if defined(USE_IMGUI)
ID3D12GraphicsCommandList* DirectXCommon::GetRecordedCommandListForImGui(Passkey<ImGuiManager>, HWND hwnd) const {
    auto it = sHwndToSwapChainIndex.find(hwnd);
    if (it == sHwndToSwapChainIndex.end()) return nullptr;
    auto* sc = sSwapChains[it->second].get();
    if (!sc) return nullptr;
    return sc->GetRecordedCommandList(Passkey<DirectXCommon>{});
}

DX12SwapChain* DirectXCommon::GetOrCreateSwapChainForImGuiViewport(Passkey<ImGuiManager>, HWND hwnd, int32_t width, int32_t height) {
    LogScope scope;
    if (!hwnd) return nullptr;

    // 破棄保留が溜まっている場合、ここで先に消化して HWND 再利用時の DXGI エラーを避ける
    DestroyPendingSwapChains();

    auto it = sHwndToSwapChainIndex.find(hwnd);
    if (it != sHwndToSwapChainIndex.end()) {
        return sSwapChains[it->second].get();
    }
    if (sFreeSwapChains.empty()) {
        Log(Translation("engine.directx.swapchain.no.free.slot"), LogSeverity::Error);
        return nullptr;
    }

    const size_t index = sFreeSwapChains.back();
    sFreeSwapChains.pop_back();

    const int32_t bufferCount = 2;
    sSwapChains[index] = std::make_unique<DX12SwapChain>(Passkey<DirectXCommon>{}, bufferCount);
    DX12SwapChain* sc = sSwapChains[index].get();

    auto &cmd = swapChainCommandObjects_[index];
    cmd.commandAllocators.clear();
    cmd.commandAllocators.resize(static_cast<size_t>(bufferCount));

    for (int i = 0; i < bufferCount; ++i) {
        HRESULT hr = dx12Device_->GetDevice()->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(cmd.commandAllocators[i].ReleaseAndGetAddressOf()));
        if (FAILED(hr)) {
            Log(Translation("engine.directx.swapchain.commandallocator.initialize.failed"), LogSeverity::Critical);
            throw std::runtime_error("Failed to create command allocator for swapchain.");
        }
    }

    if (!cmd.commandList) {
        HRESULT hr = dx12Device_->GetDevice()->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            cmd.commandAllocators[0].Get(),
            nullptr,
            IID_PPV_ARGS(cmd.commandList.ReleaseAndGetAddressOf()));
        if (FAILED(hr)) {
            Log(Translation("engine.directx.swapchain.commandlist.initialize.failed"), LogSeverity::Critical);
            throw std::runtime_error("Failed to create command list for swapchain.");
        }
        cmd.commandList->Close();
    }

    sc->BindCommandObjects(Passkey<DirectXCommon>{}, cmd.commandList.Get(), cmd.commandAllocators);

    sc->AttachWindowAndCreate(Passkey<DirectXCommon>{}, SwapChainType::ForHwnd, hwnd, width, height);

    // viewport ウィンドウは通常 HWND 用として VSync 設定に従う
    sc->SetVSyncEnabled(GetEngineSettings().rendering.defaultEnableVSync);

    sHwndToSwapChainIndex[hwnd] = index;
    return sc;
}
#endif

void DirectXCommon::DestroyPendingSwapChains() {
    // 破棄対象がある場合は必ずGPUの処理完了を待つ
    if (sPendingDestroySwapChains.empty()) return;
    WaitForFence();

    for (auto hwnd : sPendingDestroySwapChains) {
        auto it = sHwndToSwapChainIndex.find(hwnd);
        if (it != sHwndToSwapChainIndex.end()) {
            size_t index = it->second;
            if (sSwapChains[index]) {
                sSwapChains[index]->Destroy(Passkey<DirectXCommon>{});
            }

            if (index < swapChainCommandObjects_.size()) {
                auto &cmd = swapChainCommandObjects_[index];
                cmd.commandList.Reset();
                cmd.commandAllocators.clear();
            }

            sFreeSwapChains.push_back(index);
            sHwndToSwapChainIndex.erase(it);
            Log(Translation("engine.directx.swapchain.destroyed"), LogSeverity::Debug);
        } else {
            Log(Translation("engine.directx.swapchain.notfound"), LogSeverity::Warning);
        }
    }
    sPendingDestroySwapChains.clear();
}

bool DirectXCommon::WaitForFence() {
    dx12Fence_->Signal({}, dx12CommandQueue_->GetCommandQueue());
    return dx12Fence_->Wait({});
}

void DirectXCommon::ExecuteCommand() {
    for (auto &sc : sSwapChains) {
        if (sc && sc->IsCreated()) sc->EndDraw({});
    }

    std::vector<ID3D12CommandList*> lists;
    for (auto &sc : sSwapChains) {
        if (sc && sc->IsCreated() && !sc->IsDrawing()) {
            if (auto *cl = sc->GetRecordedCommandList(Passkey<DirectXCommon>{})) {
                lists.push_back(cl);
            }
        }
    }
    if (!lists.empty()) {
        dx12CommandQueue_->ExecuteCommandLists({}, lists);
    }

    for (auto &sc : sSwapChains) {
        if (sc && sc->IsCreated() && !sc->IsDrawing()) sc->Present({});
    }

    if (!lists.empty()) {
        WaitForFence();
    }

    for (auto &sc : sSwapChains) {
        if (sc && sc->IsCreated()) sc->Resize({});
    }
}

void DirectXCommon::ExecuteOneShotCommandsForTextureManager(Passkey<TextureManager>, const std::function<void(ID3D12GraphicsCommandList*)>& record) {
    if (!dx12Device_ || !dx12CommandQueue_ || !dx12Fence_) return;
    auto* device = dx12Device_->GetDevice();
    auto* queue = dx12CommandQueue_->GetCommandQueue();
    if (!device || !queue) return;

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    HRESULT hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(allocator.GetAddressOf()));
    if (FAILED(hr)) return;

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list;
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(list.GetAddressOf()));
    if (FAILED(hr)) return;

    if (record) {
        record(list.Get());
    }

    hr = list->Close();
    if (FAILED(hr)) return;

    std::vector<ID3D12CommandList*> lists;
    lists.push_back(list.Get());
    dx12CommandQueue_->ExecuteCommandLists(Passkey<DirectXCommon>{}, lists);

    dx12Fence_->Signal(Passkey<DirectXCommon>{}, queue);
    dx12Fence_->Wait(Passkey<DirectXCommon>{});
}

void DirectXCommon::ExecuteOneShotCommandsForScreenBuffer(Passkey<ScreenBuffer>, const std::function<void(ID3D12GraphicsCommandList*)>& record) {
    if (!dx12Device_ || !dx12CommandQueue_ || !dx12Fence_) return;
    auto* device = dx12Device_->GetDevice();
    auto* queue = dx12CommandQueue_->GetCommandQueue();
    if (!device || !queue) return;

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    HRESULT hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(allocator.GetAddressOf()));
    if (FAILED(hr)) return;

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list;
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(list.GetAddressOf()));
    if (FAILED(hr)) return;

    if (record) {
        record(list.Get());
    }

    hr = list->Close();
    if (FAILED(hr)) return;

    std::vector<ID3D12CommandList*> lists;
    lists.push_back(list.Get());
    dx12CommandQueue_->ExecuteCommandLists(Passkey<DirectXCommon>{}, lists);

    dx12Fence_->Signal(Passkey<DirectXCommon>{}, queue);
    dx12Fence_->Wait(Passkey<DirectXCommon>{});
}

void DirectXCommon::ExecuteExternalCommandLists(Passkey<Renderer>, const std::vector<ID3D12CommandList*>& lists) {
    if (!dx12CommandQueue_ || !dx12Fence_) return;
    if (lists.empty()) return;

    dx12CommandQueue_->ExecuteCommandLists(Passkey<DirectXCommon>{}, lists);
    WaitForFence();
}

int DirectXCommon::AcquireCommandObjectsInternal(std::vector<ScreenBufferCommandObjects>& pool, std::vector<int>& freeSlots) {
    if (!dx12Device_) return -1;
    auto* device = dx12Device_->GetDevice();
    if (!device) return -1;

    int index = -1;
    if (!freeSlots.empty()) {
        index = freeSlots.back();
        freeSlots.pop_back();
    } else {
        index = static_cast<int>(pool.size());
        pool.emplace_back();
    }

    auto& entry = pool[static_cast<size_t>(index)];

    if (!entry.commandAllocator) {
        HRESULT hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(entry.commandAllocator.ReleaseAndGetAddressOf()));
        if (FAILED(hr)) return -1;
    }

    if (!entry.commandList) {
        HRESULT hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, entry.commandAllocator.Get(), nullptr, IID_PPV_ARGS(entry.commandList.ReleaseAndGetAddressOf()));
        if (FAILED(hr)) return -1;
        entry.commandList->Close();
    }

    return index;
}

DirectXCommon::ScreenBufferCommandObjects* DirectXCommon::GetCommandObjectsInternal(std::vector<ScreenBufferCommandObjects>& pool, int slotIndex) {
    if (slotIndex < 0) return nullptr;
    const size_t idx = static_cast<size_t>(slotIndex);
    if (idx >= pool.size()) return nullptr;
    return &pool[idx];
}

void DirectXCommon::ReleaseCommandObjectsInternal(std::vector<ScreenBufferCommandObjects>& pool, std::vector<int>& freeSlots, int slotIndex) {
    if (slotIndex < 0) return;
    const size_t idx = static_cast<size_t>(slotIndex);
    if (idx >= pool.size()) return;

    // GPU 実行中の可能性があるため、ここでフェンスを待ってから解放キューへ戻す
    WaitForFence();
    freeSlots.push_back(slotIndex);
}

int DirectXCommon::AcquireScreenBufferCommandObjects(Passkey<ScreenBuffer>) {
    return AcquireCommandObjectsInternal(screenBufferCommandObjects_, freeScreenBufferCommandObjectSlots_);
}

DirectXCommon::ScreenBufferCommandObjects* DirectXCommon::GetScreenBufferCommandObjects(Passkey<ScreenBuffer>, int slotIndex) {
    return GetCommandObjectsInternal(screenBufferCommandObjects_, slotIndex);
}

void DirectXCommon::ReleaseScreenBufferCommandObjects(Passkey<ScreenBuffer>, int slotIndex) {
    ReleaseCommandObjectsInternal(screenBufferCommandObjects_, freeScreenBufferCommandObjectSlots_, slotIndex);
}

int DirectXCommon::AcquireShadowMapBufferCommandObjects(Passkey<ShadowMapBuffer>) {
    return AcquireCommandObjectsInternal(shadowMapBufferCommandObjects_, freeShadowMapBufferCommandObjectSlots_);
}

DirectXCommon::ScreenBufferCommandObjects* DirectXCommon::GetShadowMapBufferCommandObjects(Passkey<ShadowMapBuffer>, int slotIndex) {
    return GetCommandObjectsInternal(shadowMapBufferCommandObjects_, slotIndex);
}

void DirectXCommon::ReleaseShadowMapBufferCommandObjects(Passkey<ShadowMapBuffer>, int slotIndex) {
    ReleaseCommandObjectsInternal(shadowMapBufferCommandObjects_, freeShadowMapBufferCommandObjectSlots_, slotIndex);
}

} // namespace KashipanEngine