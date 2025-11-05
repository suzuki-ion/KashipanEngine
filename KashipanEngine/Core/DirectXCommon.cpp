#include "DirectXCommon.h"
#include "EngineSettings.h"
#include "Graphics/Resources.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace KashipanEngine {

namespace {
/// @brief SwapChain管理用マップ
std::unordered_map<HWND, std::unique_ptr<DX12SwapChain>> sSwapChains;
/// @brief 削除予定のSwapChainのウィンドウハンドルリスト
std::vector<HWND> sPendingDestroySwapChains;
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
    dx12Commands_ = std::make_unique<DX12Commands>(Passkey<DirectXCommon>{}, dx12Device_->GetDevice(), D3D12_COMMAND_LIST_TYPE_DIRECT);
    dx12Fence_ = std::make_unique<DX12Fence>(Passkey<DirectXCommon>{}, dx12Device_->GetDevice());

    IGraphicsResource::SetDevice({}, dx12Device_->GetDevice());
    IGraphicsResource::SetCommandList({}, dx12Commands_->GetCommandList());
    RenderTargetResource::SetCommandList({}, dx12Commands_->GetCommandList());
    DepthStencilResource::SetCommandList({}, dx12Commands_->GetCommandList());

    RTVHeap_ = std::make_unique<RTVHeap>(Passkey<DirectXCommon>{}, dx12Device_->GetDevice(), 32);
    DSVHeap_ = std::make_unique<DSVHeap>(Passkey<DirectXCommon>{}, dx12Device_->GetDevice(), 32);
    SRVHeap_ = std::make_unique<SRVHeap>(Passkey<DirectXCommon>{}, dx12Device_->GetDevice(), 256);

    auto settings = GetEngineSettings().rendering;
    RenderTargetResource::SetDefaultClearColor(Passkey<DirectXCommon>{}, settings.defaultClearColor);

    DX12SwapChain::Initialize(Passkey<DirectXCommon>{}, this,
        dx12Device_->GetDevice(),
        dx12DXGIs_->GetDXGIFactory(),
        dx12Commands_->GetCommandQueue(),
        dx12Commands_->GetCommandList(),
        RTVHeap_.get(),
        DSVHeap_.get()
    );

    Log(Translation("engine.directx.initialize.end"), LogSeverity::Debug);
}

DirectXCommon::~DirectXCommon() {
    LogScope scope;
    Log(Translation("engine.directx.finalize.start"), LogSeverity::Debug);

    // GPU 完了待ち（Live Objects を減らすため）
    if (dx12Fence_ && dx12Commands_) {
        dx12Fence_->Signal(Passkey<DirectXCommon>{}, dx12Commands_->GetCommandQueue());
        dx12Fence_->Wait(Passkey<DirectXCommon>{});
    }

    sSwapChains.clear();
    IGraphicsResource::ClearAllResources({});
    SRVHeap_.reset();
    DSVHeap_.reset();
    RTVHeap_.reset();
    dx12Fence_.reset();
    dx12Commands_.reset();
    dx12Device_.reset();
    dx12DXGIs_.reset();
    Log(Translation("engine.directx.finalize.end"), LogSeverity::Debug);
}

void DirectXCommon::BeginDraw(Passkey<GameEngine>) {
    DestroyPendingSwapChains();
}

void DirectXCommon::EndDraw(Passkey<GameEngine>) {
    ExecuteCommand();
}

DX12SwapChain *DirectXCommon::CreateSwapChain(Passkey<Window>, SwapChainType swapChainType, HWND hwnd, int32_t width, int32_t height, int32_t bufferCount) {
    LogScope scope;
    if (sSwapChains.find(hwnd) != sSwapChains.end()) {
        Log(Translation("engine.directx.swapchain.already.exists"), LogSeverity::Warning);
        return sSwapChains[hwnd].get();
    }
    auto swapChain = std::make_unique<DX12SwapChain>(Passkey<DirectXCommon>{}, swapChainType, hwnd, width, height, bufferCount);
    bool enableVSync = (swapChainType == SwapChainType::ForComposition) ?
        true :
        GetEngineSettings().rendering.defaultEnableVSync;
    swapChain->SetVSyncEnabled(enableVSync);
    DX12SwapChain *swapChainPtr = swapChain.get();
    sSwapChains[hwnd] = std::move(swapChain);
    return swapChainPtr;
}

void DirectXCommon::DestroySwapChainSignal(Passkey<Window>, HWND hwnd) {
    LogScope scope;
    if (sSwapChains.find(hwnd) == sSwapChains.end()) {
        Log(Translation("engine.directx.swapchain.notfound"), LogSeverity::Warning);
        return;
    }
    sPendingDestroySwapChains.push_back(hwnd);
}

void DirectXCommon::DestroyPendingSwapChains() {
    for (auto hwnd : sPendingDestroySwapChains) {
        auto it = sSwapChains.find(hwnd);
        if (it != sSwapChains.end()) {
            sSwapChains.erase(it);
            Log(Translation("engine.directx.swapchain.destroyed"), LogSeverity::Debug);
        } else {
            Log(Translation("engine.directx.swapchain.notfound"), LogSeverity::Warning);
        }
    }
    sPendingDestroySwapChains.clear();
}

bool DirectXCommon::WaitForFence() {
    dx12Fence_->Signal({}, dx12Commands_->GetCommandQueue());
    return dx12Fence_->Wait({});
}

void DirectXCommon::ExecuteCommand() {
    for (auto &swapChain : sSwapChains) {
        swapChain.second->EndDraw({});
    }
    dx12Commands_->ExecuteCommandList({});
    for (auto &swapChain : sSwapChains) {
        swapChain.second->Present({});
    }
    WaitForFence();
    dx12Commands_->ResetCommandAllocatorAndList({});

    // スワップチェーンのサイズ変更処理
    for (auto &swapChain : sSwapChains) {
        swapChain.second->Resize({});
    }
}

} // namespace KashipanEngine