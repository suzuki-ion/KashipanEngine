#include "DX12SwapChain.h"
#include "Core/DirectXCommon.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace KashipanEngine {

DX12SwapChain::DX12SwapChain(Passkey<DirectXCommon>, HWND hwnd, int32_t width, int32_t height, int32_t bufferCount) {
    LogScope scope;
    Log(Translation("engine.directx.swapchain.initialize.start"), LogSeverity::Debug);

    hwnd_ = hwnd;
    width_ = width;
    height_ = height;
    bufferCount_ = bufferCount;

    CreateSwapChain();
    SetViewportAndScissorRect();
    CreateBackBuffers();
    CreateDepthStencilBuffer();

    Log(Translation("engine.directx.swapchain.initialize.end"), LogSeverity::Debug);
}

DX12SwapChain::~DX12SwapChain() {
    LogScope scope;
    Log(Translation("engine.directx.swapchain.finalize.start"), LogSeverity::Debug);
    depthStencilBuffer_.reset();
    backBuffers_.clear();
    swapChain_.Reset();
    Log(Translation("engine.directx.swapchain.finalize.end"), LogSeverity::Debug);
}

void DX12SwapChain::SetViewport(float topLeftX, float topLeftY, float width, float height, float minDepth, float maxDepth) {
    viewport_.TopLeftX = topLeftX;
    viewport_.TopLeftY = topLeftY;
    viewport_.Width = width;
    viewport_.Height = height;
    viewport_.MinDepth = minDepth;
    viewport_.MaxDepth = maxDepth;
}

void DX12SwapChain::SetScissor(int32_t left, int32_t top, int32_t right, int32_t bottom) {
    scissorRect_.left = std::max(0, left);
    scissorRect_.top = std::max(0, top);
    scissorRect_.right = std::min(width_, right);
    scissorRect_.bottom = std::min(height_, bottom);
}

void DX12SwapChain::SetLetterboxViewportAndScissor(float targetAspectRatio) {
    targetAspectRatio_ = targetAspectRatio;
    float width = static_cast<float>(width_);
    float height = static_cast<float>(height_);
    float windowAspectRatio = width / height;

    float viewportX = 0.0f;
    float viewportY = 0.0f;
    float viewportWidth = width;
    float viewportHeight = height;

    if (windowAspectRatio > targetAspectRatio_) {
        // ウィンドウが横長の場合、左右にレターボックスを追加
        viewportWidth = height * targetAspectRatio_;
        viewportX = (width - viewportWidth) * 0.5f;
    } else if (windowAspectRatio < targetAspectRatio_) {
        // ウィンドウが縦長の場合、上下にレターボックスを追加
        viewportHeight = width / targetAspectRatio_;
        viewportY = (height - viewportHeight) + 0.5f;
    }
    SetViewport(viewportX, viewportY, viewportWidth, viewportHeight, 0.0f, 1.0f);

    int32_t left = static_cast<int32_t>(std::floor(viewportX));
    int32_t top = static_cast<int32_t>(std::floor(viewportY));
    int32_t right = static_cast<int32_t>(std::ceil(viewportWidth));
    int32_t bottom = static_cast<int32_t>(std::ceil(viewportHeight));
    SetScissor(left, top, right, bottom);
}

void DX12SwapChain::ResetViewportAndScissor() {
    targetAspectRatio_ = 0.0f;
    SetViewport(0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_), 0.0f, 1.0f);
    SetScissor(0, 0, width_, height_);
}

void DX12SwapChain::BeginDraw(Passkey<Window>) {
    currentBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();
    backBuffers_[currentBufferIndex_]->TransitionToNext();

    // レンダーターゲットと深度ステンシルビューの設定
    sCommandList->OMSetRenderTargets(1, &rtvHandles_[currentBufferIndex_], FALSE, &dsvHandle_);
    backBuffers_[currentBufferIndex_]->ClearRenderTargetView();
    depthStencilBuffer_->ClearDepthStencilView();

    // ビューポートとシザー矩形の設定
    sCommandList->RSSetViewports(1, &viewport_);
    sCommandList->RSSetScissorRects(1, &scissorRect_);
}

void DX12SwapChain::EndDraw(Passkey<DirectXCommon>) {
    backBuffers_[currentBufferIndex_]->TransitionToNext();
}

void DX12SwapChain::Present(Passkey<DirectXCommon>) {
    UINT syncInterval = enableVSync_ ? 1 : 0;
    UINT presentFlags = enableVSync_ ? 0 : DXGI_PRESENT_ALLOW_TEARING;
    HRESULT hr = swapChain_->Present(syncInterval, presentFlags);
    if (FAILED(hr)) {
        Log(Translation("engine.directx.swapchain.present.failed"), LogSeverity::Critical);
        throw std::runtime_error("Failed to present DX12 swap chain.");
    }
}

void DX12SwapChain::ResizeSignal(Passkey<Window>, int32_t width, int32_t height) {
    LogScope scope;
    requestedWidth_ = width;
    requestedHeight_ = height;
    isResizeRequested_ = true;
}

void DX12SwapChain::Resize(Passkey<DirectXCommon>) {
    if (!isResizeRequested_) {
        return;
    }
    LogScope scope;
    Log(Translation("engine.directx.swapchain.resize.start"), LogSeverity::Debug);
    width_ = requestedWidth_;
    height_ = requestedHeight_;

    backBuffers_.clear();
    depthStencilBuffer_.reset();

    HRESULT hr = swapChain_->ResizeBuffers(
        static_cast<UINT>(bufferCount_),
        static_cast<UINT>(width_),
        static_cast<UINT>(height_),
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
    );
    if (FAILED(hr)) {
        Log(Translation("engine.directx.swapchain.resize.failed"), LogSeverity::Critical);
        throw std::runtime_error("Failed to resize DX12 swap chain buffers.");
    }
    
    SetViewportAndScissorRect();
    CreateBackBuffers();
    CreateDepthStencilBuffer();

    // 目標のアスペクト比が設定されている場合、再計算して適用
    if (targetAspectRatio_ > 0.0f) {
        SetLetterboxViewportAndScissor(targetAspectRatio_);
    }
    
    Log(Translation("engine.directx.swapchain.resize.end"), LogSeverity::Debug);
    isResizeRequested_ = false;
}

void DX12SwapChain::CreateSwapChain() {
    //==================================================
    // スワップチェーンの生成
    //==================================================

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width = static_cast<UINT>(width_);
    swapChainDesc.Height = static_cast<UINT>(height_);
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = static_cast<UINT>(bufferCount_);
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    HRESULT hr = sDXGIFactory->CreateSwapChainForHwnd(
        sCommandQueue,
        hwnd_,
        &swapChainDesc,
        nullptr,
        nullptr,
        reinterpret_cast<IDXGISwapChain1 **>(swapChain_.GetAddressOf())
    );
    if (FAILED(hr)) {
        Log(Translation("engine.directx.swapchain.initialize.failed"), LogSeverity::Critical);
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

void DX12SwapChain::CreateBackBuffers() {
    backBuffers_.resize(bufferCount_);
    rtvHandles_.resize(bufferCount_);
    for (int32_t i = 0; i < bufferCount_; ++i) {
        Microsoft::WRL::ComPtr<ID3D12Resource> backBuffer;
        HRESULT hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(backBuffer.ReleaseAndGetAddressOf()));
        if (FAILED(hr)) {
            Log(Translation("engine.directx.swapchain.backbuffer.get.failed") + std::to_string(i), LogSeverity::Critical);
            throw std::runtime_error("Failed to get back buffer from DX12 swap chain.");
        }

        // RenderTargetResourceの作成（スワップチェーン専用）：参照は SetExistingResource 側で AddRef される
        backBuffers_[i] = std::make_unique<RenderTargetResource>(
            static_cast<UINT>(width_), static_cast<UINT>(height_),
            DXGI_FORMAT_R8G8B8A8_UNORM, sRTVHeap, backBuffer.Get());
        rtvHandles_[i] = backBuffers_[i]->GetCPUDescriptorHandle();
        backBuffers_[i]->ClearTransitionStates();
        backBuffers_[i]->AddTransitionState(D3D12_RESOURCE_STATE_PRESENT);
        backBuffers_[i]->AddTransitionState(D3D12_RESOURCE_STATE_RENDER_TARGET);
    }
}

void DX12SwapChain::CreateDepthStencilBuffer() {
    depthStencilBuffer_ = std::make_unique<DepthStencilResource>(
        static_cast<UINT>(width_), static_cast<UINT>(height_),
        DXGI_FORMAT_D24_UNORM_S8_UINT, 1.0f, static_cast<UINT8>(0), sDSVHeap);
    dsvHandle_ = depthStencilBuffer_->GetCPUDescriptorHandle();
}

} // namespace KashipanEngine