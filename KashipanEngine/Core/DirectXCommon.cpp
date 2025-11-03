#include "DirectXCommon.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace KashipanEngine {

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

    rtvHeap_ = std::make_unique<RTVHeap>(Passkey<DirectXCommon>{}, dx12Device_->GetDevice(), 32);
    dsvHeap_ = std::make_unique<DSVHeap>(Passkey<DirectXCommon>{}, dx12Device_->GetDevice(), 32);
    srvHeap_ = std::make_unique<SRVHeap>(Passkey<DirectXCommon>{}, dx12Device_->GetDevice(), 256);

    Log(Translation("engine.directx.initialize.end"), LogSeverity::Debug);
}

DirectXCommon::~DirectXCommon() {
    LogScope scope;
    Log(Translation("engine.directx.finalize.start"), LogSeverity::Debug);
    srvHeap_.reset();
    dsvHeap_.reset();
    rtvHeap_.reset();
    dx12Fence_.reset();
    dx12Commands_.reset();
    dx12Device_.reset();
    dx12DXGIs_.reset();
    Log(Translation("engine.directx.finalize.end"), LogSeverity::Debug);
}

} // namespace KashipanEngine