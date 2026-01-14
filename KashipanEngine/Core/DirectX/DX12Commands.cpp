#include "DX12Commands.h"

#include <stdexcept>

namespace KashipanEngine {

DX12Commands::DX12Commands(Passkey<DirectXCommon>, ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type) {
    if (!device) {
        throw std::runtime_error("DX12Commands: device is null");
    }

    HRESULT hr = device->CreateCommandAllocator(type, IID_PPV_ARGS(commandAllocator_.ReleaseAndGetAddressOf()));
    if (FAILED(hr)) {
        throw std::runtime_error("DX12Commands: failed to create command allocator");
    }

    hr = device->CreateCommandList(0, type, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(commandList_.ReleaseAndGetAddressOf()));
    if (FAILED(hr)) {
        throw std::runtime_error("DX12Commands: failed to create command list");
    }

    commandList_->Close();
}

ID3D12GraphicsCommandList* DX12Commands::BeginRecord() {
    if (isRecording_) return commandList_.Get();
    if (!commandAllocator_ || !commandList_) return nullptr;

    HRESULT hr = commandAllocator_->Reset();
    if (FAILED(hr)) return nullptr;

    hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
    if (FAILED(hr)) return nullptr;

    isRecording_ = true;
    isRecorded_ = false;
    return commandList_.Get();
}

bool DX12Commands::EndRecord() {
    if (!isRecording_) return false;
    if (!commandList_) return false;

    HRESULT hr = commandList_->Close();
    if (FAILED(hr)) return false;

    isRecording_ = false;
    isRecorded_ = true;
    return true;
}

} // namespace KashipanEngine
