#include "DX12SwapChain.h"
#include "Core/DirectXCommon.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>

#pragma comment(lib, "dcomp.lib")

namespace KashipanEngine {

void DX12SwapChain::AttachWindowAndCreate(Passkey<DirectXCommon>, SwapChainType swapChainType, HWND hwnd, int32_t width, int32_t height) {
    LogScope scope;
    if (isCreated_) {
        Log(Translation("engine.directx.swapchain.already.exists"), LogSeverity::Warning);
        return;
    }
    Log(Translation("engine.directx.swapchain.initialize.start"), LogSeverity::Debug);

    swapChainType_ = swapChainType;
    hwnd_ = hwnd;
    width_ = width;
    height_ = height;

    if (swapChainType_ == SwapChainType::ForHwnd) {
        CreateSwapChainForHWND();
    } else if (swapChainType_ == SwapChainType::ForComposition) {
        CreateSwapChainForComposition();
    } else {
        Log(Translation("engine.directx.swapchain.create.invalid.type"), LogSeverity::Critical);
        throw std::runtime_error("Invalid swap chain type.");
    }

    SetViewportAndScissorRect();
    CreateBackBuffers();
    CreateDepthStencilBuffer();

    isCreated_ = true;
    Log(Translation("engine.directx.swapchain.initialize.end"), LogSeverity::Debug);
}

void DX12SwapChain::Destroy(Passkey<DirectXCommon>) {
    LogScope scope;
    if (!isCreated_) return;
    Log(Translation("engine.directx.swapchain.finalize.start"), LogSeverity::Debug);
    DestroyInternal();
    swapChainType_ = SwapChainType::Unknown;
    hwnd_ = nullptr;
    width_ = height_ = 0;
    isCreated_ = false;
    Log(Translation("engine.directx.swapchain.finalize.end"), LogSeverity::Debug);
}

void DX12SwapChain::DestroyInternal() {
    depthStencilBuffer_.reset();
    backBuffers_.clear();
    swapChain_.Reset();
    dcompHost_.reset();
    currentBufferIndex_ = 0;
    sDirectXCommon->ReleaseCommandObjects(Passkey<DX12SwapChain>{}, slotIndex_);
}

void DX12SwapChain::BindCommandObjects(Passkey<DirectXCommon>,
    DX12Commands *commands, int slotIndex) {
    slotIndex_ = slotIndex;
    commands_ = commands;
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
        viewportWidth = height * targetAspectRatio_;
        viewportX = (width - viewportWidth) * 0.5f;
    } else if (windowAspectRatio < targetAspectRatio_) {
        viewportHeight = width / targetAspectRatio_;
        viewportY = (height - viewportHeight) * 0.5f;
    }
    SetViewport(viewportX, viewportY, viewportWidth, viewportHeight, 0.0f, 1.0f);

    int32_t left = static_cast<int32_t>(std::floor(viewportX));
    int32_t top = static_cast<int32_t>(std::floor(viewportY));
    int32_t right = left + static_cast<int32_t>(std::ceil(viewportWidth));
    int32_t bottom = top + static_cast<int32_t>(std::ceil(viewportHeight));
    SetScissor(left, top, right, bottom);
}

void DX12SwapChain::ResetViewportAndScissor() {
    targetAspectRatio_ = 0.0f;
    SetViewport(0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_), 0.0f, 1.0f);
    SetScissor(0, 0, width_, height_);
}

void DX12SwapChain::BeginDrawInternal() {
    if (!isCreated_ || isDrawing_) return;
    if (!commands_) {
        Log(Translation("engine.directx.swapchain.commandlist.reset.failed"), LogSeverity::Critical);
        throw std::runtime_error("Command objects are not bound to DX12SwapChain.");
    }

    currentBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();

    auto* cmd = commands_->BeginRecord();
    if (!cmd) {
        Log(Translation("engine.directx.swapchain.commandlist.reset.failed"), LogSeverity::Critical);
        throw std::runtime_error("Failed to begin record command list in DX12 swap chain.");
    }

    if (depthStencilBuffer_) depthStencilBuffer_->SetCommandList(cmd);
    for (auto &bb : backBuffers_) bb->SetCommandList(cmd);

    backBuffers_[currentBufferIndex_]->TransitionToNext();

    cmd->OMSetRenderTargets(1, &rtvHandles_[currentBufferIndex_], FALSE, &dsvHandle_);
    backBuffers_[currentBufferIndex_]->ClearRenderTargetView();
    depthStencilBuffer_->ClearDepthStencilView();

    cmd->RSSetViewports(1, &viewport_);
    cmd->RSSetScissorRects(1, &scissorRect_);
    ID3D12DescriptorHeap *ppHeaps[] = { sSRVHeap->GetDescriptorHeap(), sSamplerHeap->GetDescriptorHeap() };
    cmd->SetDescriptorHeaps(2, ppHeaps);

    isDrawing_ = true;
}

void DX12SwapChain::BeginDraw(Passkey<Window>) {
    BeginDrawInternal();
}

#if defined(USE_IMGUI)
void DX12SwapChain::BeginDraw(Passkey<ImGuiManager>) {
    BeginDrawInternal();
}
#endif

void DX12SwapChain::EndDraw(Passkey<DirectXCommon>) {
    if (!isCreated_ || !isDrawing_) return;
    backBuffers_[currentBufferIndex_]->TransitionToNext();

    if (!commands_->EndRecord()) {
        Log(Translation("engine.directx.commandlist.close.failed"), LogSeverity::Critical);
        throw std::runtime_error("Failed to close command list in DX12 swap chain.");
    }
    sDirectXCommon->AddRecordCommandList(Passkey<DX12SwapChain>{}, commands_->GetCommandList());

    isDrawing_ = false;
}

void DX12SwapChain::Present(Passkey<DirectXCommon>) {
    if (!isCreated_) return;
    UINT syncInterval = 1;
    UINT presentFlags = 0;

    if (swapChainType_ == SwapChainType::ForHwnd) {
        syncInterval = enableVSync_ ? 1 : 0;
        presentFlags = enableVSync_ ? 0 : DXGI_PRESENT_ALLOW_TEARING;
    }

    HRESULT hr = swapChain_->Present(syncInterval, presentFlags);
    if (FAILED(hr)) {
        Log(Translation("engine.directx.swapchain.present.failed"), LogSeverity::Critical);
        throw std::runtime_error("Failed to present DX12 swap chain.");
    }
}

void DX12SwapChain::ResizeSignalInternal(int32_t width, int32_t height) {
    if (!isCreated_) return;
    LogScope scope;
    requestedWidth_ = width;
    requestedHeight_ = height;
    isResizeRequested_ = true;
}

void DX12SwapChain::ResizeSignal(Passkey<Window>, int32_t width, int32_t height) {
    ResizeSignalInternal(width, height);
}

#if defined(USE_IMGUI)
void DX12SwapChain::ResizeSignal(Passkey<ImGuiManager>, int32_t width, int32_t height) {
    ResizeSignalInternal(width, height);
}
#endif

void DX12SwapChain::Resize(Passkey<DirectXCommon>) {
    if (!isCreated_ || !isResizeRequested_) return;
    LogScope scope;
    Log(Translation("engine.directx.swapchain.resize.start"), LogSeverity::Debug);
    width_ = requestedWidth_;
    height_ = requestedHeight_;

    backBuffers_.clear();
    depthStencilBuffer_.reset();

    UINT flags = 0;
    if (swapChainType_ == SwapChainType::ForHwnd) {
        flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    }

    HRESULT hr = swapChain_->ResizeBuffers(
        static_cast<UINT>(bufferCount_),
        static_cast<UINT>(width_),
        static_cast<UINT>(height_),
        DXGI_FORMAT_UNKNOWN,
        flags
    );
    if (FAILED(hr)) {
        Log(Translation("engine.directx.swapchain.resize.failed"), LogSeverity::Critical);
        throw std::runtime_error("Failed to resize DX12 swap chain buffers.");
    }

    SetViewportAndScissorRect();
    CreateBackBuffers();
    CreateDepthStencilBuffer();

    if (targetAspectRatio_ > 0.0f) {
        SetLetterboxViewportAndScissor(targetAspectRatio_);
    }

    Log(Translation("engine.directx.swapchain.resize.end"), LogSeverity::Debug);
    isResizeRequested_ = false;
}

void DX12SwapChain::CreateSwapChainForHWND() {
    LogScope scope;
    Log(Translation("engine.directx.swapchain.initialize.target.hwnd") + std::to_string(reinterpret_cast<uintptr_t>(hwnd_)), LogSeverity::Debug);

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width = static_cast<UINT>(width_);
    swapChainDesc.Height = static_cast<UINT>(height_);
    swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    backBufferFormat_ = swapChainDesc.Format;
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

void DX12SwapChain::CreateSwapChainForComposition() {
    LONG exStyle = GetWindowLong(hwnd_, GWL_EXSTYLE);
    exStyle |= WS_EX_NOREDIRECTIONBITMAP;
    SetWindowLong(hwnd_, GWL_EXSTYLE, exStyle);
    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    dcompHost_ = std::make_unique<DCompHost>(Passkey<DX12SwapChain>{});
    if (!dcompHost_->InitializeForHwnd(hwnd_, TRUE)) {
        Log(Translation("engine.directx.dcomp.initialize.failed"), LogSeverity::Critical);
        throw std::runtime_error("Failed to initialize DirectComposition host.");
    }

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = static_cast<UINT>(width_);
    desc.Height = static_cast<UINT>(height_);
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    backBufferFormat_ = desc.Format;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = static_cast<UINT>(bufferCount_);
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    desc.Flags = 0;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> sc1;
    HRESULT hr = sDXGIFactory->CreateSwapChainForComposition(
        sCommandQueue,
        &desc,
        nullptr,
        sc1.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        Log(Translation("engine.directx.swapchain.initialize.failed"), LogSeverity::Critical);
        throw std::runtime_error("Failed to create DX12 composition swap chain.");
    }

    hr = sc1.As(&swapChain_);
    if (FAILED(hr)) {
        Log(Translation("engine.directx.swapchain.initialize.failed"), LogSeverity::Critical);
        throw std::runtime_error("Failed to query IDXGISwapChain4 for composition.");
    }

    if (!dcompHost_->SetContentSwapChain(sc1.Get())) {
        Log(Translation("engine.directx.dcomp.setcontent.failed"), LogSeverity::Critical);
        throw std::runtime_error("Failed to set composition swap chain as content.");
    }
    if (!dcompHost_->Commit()) {
        Log(Translation("engine.directx.dcomp.commit.failed"), LogSeverity::Critical);
        throw std::runtime_error("Failed to commit DirectComposition changes.");
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

        auto desc = backBuffer->GetDesc();

        if (swapChainType_ == SwapChainType::ForHwnd) {
            backBuffers_[i] = std::make_unique<RenderTargetResource>(
                static_cast<UINT>(width_), static_cast<UINT>(height_),
                desc.Format, backBuffer.Get());
        } else { // Composition
#if defined(DEBUG_BUILD) || defined(DEVELOPMENT_BUILD)
            float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.5f };
#else
            float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
#endif
            backBuffers_[i] = std::make_unique<RenderTargetResource>(
                static_cast<UINT>(width_), static_cast<UINT>(height_),
                desc.Format, backBuffer.Get(), clearColor);
        }

        rtvHandles_[i] = backBuffers_[i]->GetCPUDescriptorHandle();
        backBuffers_[i]->ClearTransitionStates();
        backBuffers_[i]->AddTransitionState(D3D12_RESOURCE_STATE_PRESENT);
        backBuffers_[i]->AddTransitionState(D3D12_RESOURCE_STATE_RENDER_TARGET);
    }
}

void DX12SwapChain::CreateDepthStencilBuffer() {
    depthStencilBuffer_ = std::make_unique<DepthStencilResource>(
        static_cast<UINT>(width_), static_cast<UINT>(height_),
        DXGI_FORMAT_D24_UNORM_S8_UINT, 1.0f, static_cast<UINT8>(0));
    dsvHandle_ = depthStencilBuffer_->GetCPUDescriptorHandle();
}

} // namespace KashipanEngine