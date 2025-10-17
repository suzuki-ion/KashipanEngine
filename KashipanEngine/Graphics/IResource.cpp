#include "IResource.h"
#include <cassert>

namespace KashipanEngine {

// 静的メンバの定義
DirectXDevice *IResource::dxDevice_ = nullptr;
bool IResource::isCommonInitialized_ = false;

void IResource::TransitionTo(D3D12_RESOURCE_STATES newState) {
    if (currentState_ != newState && resource_) {
        SetResourceBarrier(currentState_, newState);
        currentState_ = newState;
    }
}

void IResource::SetResourceBarrier(D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState) {
    assert(isCommonInitialized_);
    assert(resource_);

    // DirectXDevice からコマンドリストを取得できることを前提
    auto *commandList = dxDevice_->GetCommandList();
    assert(commandList);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource_.Get();
    barrier.Transition.StateBefore = beforeState;
    barrier.Transition.StateAfter = afterState;

    commandList->ResourceBarrier(1, &barrier);
}

Microsoft::WRL::ComPtr<ID3D12Resource> IResource::CreateCommittedResource(
    const D3D12_HEAP_PROPERTIES &heapProps,
    D3D12_HEAP_FLAGS heapFlags,
    const D3D12_RESOURCE_DESC &resourceDesc,
    D3D12_RESOURCE_STATES initialState,
    const D3D12_CLEAR_VALUE *clearValue) {

    assert(isCommonInitialized_);

    Microsoft::WRL::ComPtr<ID3D12Resource> resource;

    auto *device = dxDevice_->GetDevice();
    assert(device);

    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        heapFlags,
        &resourceDesc,
        initialState,
        clearValue,
        IID_PPV_ARGS(&resource)
    );

    assert(SUCCEEDED(hr));
    return resource;
}

UINT IResource::GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE heapType) const {
    assert(isCommonInitialized_);
    auto *device = dxDevice_->GetDevice();
    assert(device);
    return device->GetDescriptorHandleIncrementSize(heapType);
}

} // namespace KashipanEngine