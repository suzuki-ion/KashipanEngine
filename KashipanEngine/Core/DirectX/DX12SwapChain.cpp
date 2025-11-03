#include "DX12SwapChain.h"
#include <stdexcept>

namespace KashipanEngine {

DX12SwapChain::DX12SwapChain(Passkey<DirectXCommon>, ID3D12Device *device, IDXGIFactory7 *dxgiFactory, ID3D12CommandQueue *commandQueue,
    HWND hwnd, int32_t width, int32_t height, int32_t bufferCount) {
    LogScope scope;
    Log(Translation("engine.directx.swapchain.initialize.start"), LogSeverity::Debug);
    
    static_cast<void>(device);
    bufferCount_ = bufferCount;
    currentBufferIndex_ = 0;
    width_ = width;
    height_ = height;

    CreateSwapChain(dxgiFactory, commandQueue, hwnd);
    SetViewportAndScissorRect();

    Log(Translation("engine.directx.swapchain.initialize.end"), LogSeverity::Debug);
}

DX12SwapChain::~DX12SwapChain() {
    LogScope scope;
    Log(Translation("engine.directx.swapchain.finalize.start"), LogSeverity::Debug);
    depthStencilBuffer_.Reset();
    rtvHandles_.clear();
    backBuffers_.clear();
    swapChain_.Reset();
    Log(Translation("engine.directx.swapchain.finalize.end"), LogSeverity::Debug);
}

void DX12SwapChain::CreateSwapChain(IDXGIFactory7 *dxgiFactory, ID3D12CommandQueue *commandQueue, HWND hwnd) {
    //==================================================
    // スワップチェーンの生成
    //==================================================

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width = static_cast<UINT>(width_);
    swapChainDesc.Height = static_cast<UINT>(height_);
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = static_cast<UINT>(bufferCount_);
    swapChainDesc.Scaling = DXGI_SCALING_ASPECT_RATIO_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    HRESULT hr = dxgiFactory->CreateSwapChainForHwnd(
        commandQueue,
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        reinterpret_cast<IDXGISwapChain1 **>(swapChain_.GetAddressOf())
    );
    if (FAILED(hr)) {
        Log(Translation("engine.directx.swapchain.initialize.failed"), LogSeverity::Error);
        throw std::runtime_error("Failed to create DX12 swap chain.");
    }
}

void DX12SwapChain::SetViewportAndScissorRect() {
    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
    viewport_.Width = static_cast<FLOAT>(width_);
    viewport_.Height = static_cast<FLOAT>(height_);

    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;
    scissorRect_.left = 0;
    scissorRect_.top = 0;
    scissorRect_.right = width_;
    scissorRect_.bottom = height_;
}

void DX12SwapChain::CreateBackBuffers() {}

void DX12SwapChain::CreateDepthStencilBuffer() {}

} // namespace KashipanEngine