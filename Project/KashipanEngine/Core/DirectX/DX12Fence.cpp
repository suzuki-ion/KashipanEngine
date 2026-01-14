#include "DX12Fence.h"
#include <stdexcept>

namespace KashipanEngine {

DX12Fence::DX12Fence(Passkey<DirectXCommon>, ID3D12Device *device) {
    LogScope scope;
    Log(Translation("engine.directx.fence.initialize.start"), LogSeverity::Debug);

    HRESULT hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
    if (FAILED(hr)) {
        Log(Translation("engine.directx.fence.initialize.failed"), LogSeverity::Critical);
        throw std::runtime_error("Failed to create DX12 fence.");
    }
    
    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (fenceEvent_ == nullptr) {
        Log(Translation("engine.directx.fence.event.create.failed"), LogSeverity::Critical);
        throw std::runtime_error("Failed to create fence event.");
    }
    currentValue_ = 0;

    Log(Translation("engine.directx.fence.initialize.end"), LogSeverity::Debug);
}

DX12Fence::~DX12Fence() {
    LogScope scope;
    Log(Translation("instance.destroying"), LogSeverity::Debug);
    CloseHandle(fenceEvent_);
    fence_.Reset();
    Log(Translation("instance.destroyed"), LogSeverity::Debug);
}

void DX12Fence::Signal(Passkey<DirectXCommon>, ID3D12CommandQueue *commandQueue) {
    currentValue_++;
    HRESULT hr = commandQueue->Signal(fence_.Get(), currentValue_);
    if (FAILED(hr)) {
        Log(Translation("engine.directx.fence.signal.failed"), LogSeverity::Critical);
        throw std::runtime_error("Failed to signal DX12 fence.");
    }
}

bool DX12Fence::Wait(Passkey<DirectXCommon>) {
    if (fence_->GetCompletedValue() < currentValue_) {
        HRESULT hr = fence_->SetEventOnCompletion(currentValue_, fenceEvent_);
        if (FAILED(hr)) {
            Log(Translation("engine.directx.fence.wait.failed"), LogSeverity::Critical);
            throw std::runtime_error("Failed to set event on completion for DX12 fence.");
        }
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
    return true;
}

} // namespace KashipanEngine