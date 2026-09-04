#include "DescriptorHeapBase.h"
#include <stdexcept>
#include "Utilities/Translation.h"

namespace KashipanEngine {

DescriptorHandleInfo::~DescriptorHandleInfo() {
    LogScope scope;
    owner_->FreeDescriptorHandle({}, index);
}

DescriptorHeapBase::~DescriptorHeapBase() {
    LogScope scope;
    Log(Translation("instance.destroying"), LogSeverity::Debug);
    descriptorHeap_.Reset();
    Log(Translation("instance.destroyed"), LogSeverity::Debug);
}

std::unique_ptr<DescriptorHandleInfo> DescriptorHeapBase::AllocateFrom(std::vector<uint32_t> &pool) {
    LogScope scope;
    if (pool.empty()) {
        Log(Translation("engine.directx.descriptorheap.allocation.failed"), LogSeverity::Critical);
        throw std::runtime_error("No more free descriptor handles available.");
    }

    // 空いているデスクリプタインデックスを取得
    UINT index = pool.back();
    pool.pop_back();

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

std::unique_ptr<DescriptorHandleInfo> DescriptorHeapBase::AllocateDescriptorHandle() {
    LogScope scope;
    return AllocateFrom(freeIndices_);
}

std::unique_ptr<DescriptorHandleInfo> DescriptorHeapBase::AllocateReservedDescriptorHandle() {
    LogScope scope;
    return AllocateFrom(freeReservedIndices_);
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeapBase::GetReservedRangeBaseGpuHandle() const noexcept {
    LogScope scope;
    D3D12_GPU_DESCRIPTOR_HANDLE handle{};
    if (reservedCount_ == 0 || !isShaderVisible_) return handle;
    // 予約レンジは常にヒープ先頭（インデックス0）から確保されるため、ヒープ先頭ハンドルがそのままベースになる
    return descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
}

void DescriptorHeapBase::FreeDescriptorHandle(Passkey<DescriptorHandleInfo>, UINT index) {
    LogScope scope;
    if (index >= numDescriptors_) {
        Log(Translation("engine.directx.descriptorheap.free.failed"), LogSeverity::Critical);
        throw std::runtime_error("Invalid descriptor index to free.");
    }
    // 解放済みデスクリプタインデックスを、元のプール（予約レンジ/汎用）へ戻す
    if (index < reservedCount_) {
        freeReservedIndices_.push_back(index);
    } else {
        freeIndices_.push_back(index);
    }
}

DescriptorHeapBase::DescriptorHeapBase(ID3D12Device *device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_FLAGS flags, UINT reservedCount) {
    LogScope scope;
    Log(Translation("engine.directx.descriptorheap.initialize.start"), LogSeverity::Debug);

    device_ = device;
    type_ = type;
    numDescriptors_ = numDescriptors;
    reservedCount_ = reservedCount;
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
    // 予約レンジ [0, reservedCount_) と汎用プール [reservedCount_, numDescriptors_) を別々のフリーリストに分離する。
    // どちらも従来通り「後ろから割り当てる」（LIFOポップで昇順に払い出される）
    freeReservedIndices_.reserve(reservedCount_);
    for (UINT i = 0; i < reservedCount_; ++i) {
        freeReservedIndices_.push_back(reservedCount_ - 1 - i);
    }
    const UINT generalCount = numDescriptors_ - reservedCount_;
    freeIndices_.reserve(generalCount);
    for (UINT i = 0; i < generalCount; ++i) {
        freeIndices_.push_back(numDescriptors_ - 1 - i);
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

} // namespace KashipanEngine