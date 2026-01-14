#include "SamplerManager.h"

#include "Core/DirectXCommon.h"
#include "Graphics/Pipeline/System/ShaderVariableBinder.h"
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

} // namespace

SamplerManager::SamplerManager(Passkey<GameEngine>, DirectXCommon* directXCommon) : directXCommon_(directXCommon) {
    LogScope scope;
    if (directXCommon_) {
        sDevice = directXCommon_->GetDeviceForSamplerManager(Passkey<SamplerManager>{});
        sSamplerHeap = directXCommon_->GetSamplerHeapForSamplerManager(Passkey<SamplerManager>{});
    }

    // デフォルトのサンプラーを作成しておく
    D3D12_SAMPLER_DESC defaultSamplerDesc{};
    defaultSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    defaultSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    defaultSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    defaultSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    defaultSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    defaultSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
    defaultSamplerDesc.MaxAnisotropy = 1;
    CreateSampler(defaultSamplerDesc);
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

bool SamplerManager::BindSampler(ShaderVariableBinder* shaderBinder, const std::string& nameKey, SamplerHandle handle) {
    if (!shaderBinder) return false;
    if (handle == kInvalidHandle) return false;

    auto it = sSamplers.find(handle);
    if (it == sSamplers.end()) return false;

    D3D12_GPU_DESCRIPTOR_HANDLE h{};
    h.ptr = it->second.gpuPtr;
    if (h.ptr == 0) return false;

    return shaderBinder->Bind(nameKey, h);
}

} // namespace KashipanEngine
