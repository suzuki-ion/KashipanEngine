#include "DirectXCommon.h"
#include "EngineSettings.h"
#include "Graphics/Resources.h"
#include <vector>
#include <unordered_map>

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

    // グローバルヒープポインタ設定
    IGraphicsResource::SetDescriptorHeaps({},
        RTVHeap_.get(), DSVHeap_.get(), SRVHeap_.get());
    
    DX12SwapChain::Initialize(Passkey<DirectXCommon>{}, this,
        dx12Device_->GetDevice(),
        dx12DXGIs_->GetDXGIFactory(),
        dx12CommandQueue_->GetCommandQueue(),
        RTVHeap_.get(),
        DSVHeap_.get(),
        SRVHeap_.get()
    );

    // スワップチェーンスロット事前確保 (遅延初期化用空インスタンス生成)
    size_t maxWindows = GetEngineSettings().limits.maxWindows;
    sSwapChains.resize(maxWindows);
    sFreeSwapChains.reserve(maxWindows);
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

    IGraphicsResource::ClearAllResources({});
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

} // namespace KashipanEngine