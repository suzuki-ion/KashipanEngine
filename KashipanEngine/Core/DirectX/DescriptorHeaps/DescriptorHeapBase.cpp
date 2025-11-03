#include "DescriptorHeapBase.h"
#include <stdexcept>

namespace KashipanEngine {

DescriptorHandleInfo::~DescriptorHandleInfo() {
    owner_->FreeDescriptorHandle({}, index);
}

DescriptorHeapBase::DescriptorHeapBase(Passkey<DirectXCommon>, ID3D12Device *device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_FLAGS flags) {
    LogScope scope;
    Log(Translation("engine.directx.descriptorheap.initialize.start"), LogSeverity::Debug);

    device_ = device;
    type_ = type;
    numDescriptors_ = numDescriptors;
    isShaderVisible_ = (flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) != 0;

    // デスクリプタヒープの作成
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = type_;
    heapDesc.NumDescriptors = numDescriptors_;
    heapDesc.Flags = flags;
    heapDesc.NodeMask = 0;
    HRESULT hr = device_->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap_));
    if (FAILED(hr)) {
        Log(Translation("engine.directx.descriptorheap.initialize.failed"), LogSeverity::Critical);
        throw std::runtime_error("Failed to create descriptor heap.");
    }

    // 解放済みデスクリプタインデックスの初期化
    freeIndices_.reserve(numDescriptors_);
    for (UINT i = 0; i < numDescriptors_; ++i) {
        freeIndices_.push_back(numDescriptors_ - 1 - i); // 後ろから割り当てる
    }

    std::string logText;
    switch (type_) {
        case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
            logText = Translation("engine.directx.descriptorheap.initialize.type.cbvsvuav");
            break;
        case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
            logText = Translation("engine.directx.descriptorheap.initialize.type.sampler");
            break;
        case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:
            logText = Translation("engine.directx.descriptorheap.initialize.type.rtv");
            break;
        case D3D12_DESCRIPTOR_HEAP_TYPE_DSV:
            logText = Translation("engine.directx.descriptorheap.initialize.type.dsv");
            break;
        default:
            break;
    }
    logText += std::to_string(numDescriptors_);
    Log(logText, LogSeverity::Debug);

    Log(Translation("engine.directx.descriptorheap.initialize.end"), LogSeverity::Debug);
}

DescriptorHeapBase::~DescriptorHeapBase() {
    LogScope scope;
    Log(Translation("instance.destroying"), LogSeverity::Debug);
    descriptorHeap_.Reset();
    Log(Translation("instance.destroyed"), LogSeverity::Debug);
}

std::unique_ptr<DescriptorHandleInfo> DescriptorHeapBase::AllocateDescriptorHandle() {
    LogScope scope;
    if (freeIndices_.empty()) {
        Log(Translation("engine.directx.descriptorheap.allocation.failed"), LogSeverity::Critical);
        throw std::runtime_error("No more free descriptor handles available.");
    }

    // 空いているデスクリプタインデックスを取得
    UINT index = freeIndices_.back();
    freeIndices_.pop_back();

    // デスクリプタハンドルの計算
    SIZE_T descriptorSize = device_->GetDescriptorHandleIncrementSize(type_);
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    cpuHandle.ptr += index * descriptorSize;
    
    // GPUデスクリプタハンドルの計算（シェーダー可視の場合のみ）
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = {};
    if (isShaderVisible_) {
        gpuHandle = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
        gpuHandle.ptr += index * descriptorSize;
    }

    // デスクリプタハンドル情報の作成
    return std::make_unique<DescriptorHandleInfo>(Passkey<DescriptorHeapBase>{}, this, index, cpuHandle, gpuHandle);
}

void DescriptorHeapBase::FreeDescriptorHandle(Passkey<DescriptorHandleInfo>, UINT index) {
    LogScope scope;
    if (index >= numDescriptors_) {
        Log(Translation("engine.directx.descriptorheap.free.failed"), LogSeverity::Critical);
        throw std::runtime_error("Invalid descriptor index to free.");
    }
    // 解放済みデスクリプタインデックスに追加
    freeIndices_.push_back(index);
}

} // namespace KashipanEngine