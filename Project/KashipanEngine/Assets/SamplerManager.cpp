#include "SamplerManager.h"

#include "Core/DirectXCommon.h"
#include "Graphics/Pipeline/System/ShaderVariableBinder.h"
#include "Assets/DefaultSamplerDescs.h"
#include "Debug/Logger.h"

#include <unordered_map>
#include <memory>

namespace KashipanEngine {

namespace {

using Handle = SamplerManager::SamplerHandle;

struct SamplerEntry {
    std::unique_ptr<DescriptorHandleInfo> desc;
    UINT64 gpuPtr = 0;
    UINT index = 0;
};

std::unordered_map<Handle, SamplerEntry> sSamplers;
SamplerHeap* sSamplerHeap = nullptr;
ID3D12Device* sDevice = nullptr;

Handle RegisterEntry(SamplerEntry&& entry) {
    // handle 0 is invalid; return index+1
    const Handle h = static_cast<Handle>(entry.index + 1u);
    if (h == SamplerManager::kInvalidHandle) return SamplerManager::kInvalidHandle;
    if (sSamplers.find(h) != sSamplers.end()) return SamplerManager::kInvalidHandle;
    sSamplers.emplace(h, std::move(entry));
    return h;
}

/// @brief 既定サンプラー専用（SamplerHeap先頭の予約レンジ）にサンプラーを確保する。
///        gSamplers[6]（バインドレスサンプラー配列、DefaultSamplerDescs.h参照）のインデックスと
///        ヒープ上の位置を一致させるため、コンストラクタでの既定6種の生成にのみ使う
Handle CreateReservedSampler(const D3D12_SAMPLER_DESC &desc) {
    if (!sDevice || !sSamplerHeap) return SamplerManager::kInvalidHandle;

    auto handleInfo = sSamplerHeap->AllocateReservedDescriptorHandle();
    if (!handleInfo) return SamplerManager::kInvalidHandle;

    sDevice->CreateSampler(&desc, handleInfo->cpuHandle);

    SamplerEntry e{};
    e.gpuPtr = handleInfo->gpuHandle.ptr;
    e.index = handleInfo->index;
    e.desc = std::move(handleInfo);

    return RegisterEntry(std::move(e));
}

} // namespace

SamplerManager::SamplerManager(Passkey<GameEngine>, DirectXCommon* directXCommon) : directXCommon_(directXCommon) {
    LogScope scope;
    if (directXCommon_) {
        sDevice = directXCommon_->GetDeviceForSamplerManager(Passkey<SamplerManager>{});
        sSamplerHeap = directXCommon_->GetSamplerHeapForSamplerManager(Passkey<SamplerManager>{});
    }

    // デフォルトのサンプラーを作成しておく（gSamplers[6]（バインドレスサンプラー配列）生成と共通の
    // 定義元（DefaultSamplerDescs.h）から、DefaultSampler enumと同じ並び順で生成する）。
    // SamplerHeap先頭の予約レンジ（DirectXCommon参照）へ確保することで、常にインデックス0〜5に
    // 収まることを保証する（gSamplers[]のバインドレステーブルはこの予約レンジ先頭を指す）
    for (const auto &d : GetDefaultSamplerDescs()) {
        D3D12_SAMPLER_DESC desc{};
        desc.Filter = d.filter;
        desc.AddressU = d.addressMode;
        desc.AddressV = d.addressMode;
        desc.AddressW = d.addressMode;
        desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        desc.MaxLOD = D3D12_FLOAT32_MAX;
        desc.MaxAnisotropy = d.maxAnisotropy;
        CreateReservedSampler(desc);
    }
}

SamplerManager::~SamplerManager() {
    LogScope scope;
    sSamplers.clear();
    sSamplerHeap = nullptr;
    sDevice = nullptr;
}

SamplerManager::SamplerHandle SamplerManager::CreateSampler(const D3D12_SAMPLER_DESC& desc) {
    LogScope scope;
    if (!sDevice || !sSamplerHeap) return kInvalidHandle;

    auto handleInfo = sSamplerHeap->AllocateDescriptorHandle();
    if (!handleInfo) return kInvalidHandle;

    sDevice->CreateSampler(&desc, handleInfo->cpuHandle);

    SamplerEntry e{};
    e.gpuPtr = handleInfo->gpuHandle.ptr;
    e.index = handleInfo->index;
    e.desc = std::move(handleInfo);

    return RegisterEntry(std::move(e));
}

SamplerManager::SamplerHandle SamplerManager::GetSampler(DefaultSampler state) {
    LogScope scope;
    switch (state) {
    case DefaultSampler::PointClamp:
        return 1;
    case DefaultSampler::PointWrap:
        return 2;
    case DefaultSampler::LinearClamp:
        return 3;
    case DefaultSampler::LinearWrap:
        return 4;
    case DefaultSampler::AnisotropicClamp:
        return 5;
    case DefaultSampler::AnisotropicWrap:
        return 6;
    default:
        return kInvalidHandle;
    }
}

bool SamplerManager::BindSampler(ShaderVariableBinder* shaderBinder, const std::string& nameKey, SamplerHandle handle) {
    LogScope scope;
    if (!shaderBinder) return false;
    if (handle == kInvalidHandle) return false;

    auto it = sSamplers.find(handle);
    if (it == sSamplers.end()) return false;

    D3D12_GPU_DESCRIPTOR_HANDLE h{};
    h.ptr = it->second.gpuPtr;
    if (h.ptr == 0) return false;

    return shaderBinder->Bind(nameKey, h);
}

bool SamplerManager::BindSampler(ShaderVariableBinder *shaderBinder, const std::string &nameKey, DefaultSampler defaultSampler) {
    LogScope scope;
    if (!shaderBinder) return false;
    Handle handle = kInvalidHandle;
    switch (defaultSampler) {
    case DefaultSampler::PointClamp:
        handle = 1;
        break;
    case DefaultSampler::PointWrap:
        handle = 2;
        break;
    case DefaultSampler::LinearClamp:
        handle = 3;
        break;
    case DefaultSampler::LinearWrap:
        handle = 4;
        break;
    case DefaultSampler::AnisotropicClamp:
        handle = 5;
        break;
    case DefaultSampler::AnisotropicWrap:
        handle = 6;
        break;
    default:
        return false;
    }
    return BindSampler(shaderBinder, nameKey, handle);
}

} // namespace KashipanEngine
