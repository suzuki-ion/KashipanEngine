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
    D3D12_SAMPLER_DESC pointClamp{};
    pointClamp.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    pointClamp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    pointClamp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    pointClamp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    pointClamp.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    pointClamp.MaxLOD = D3D12_FLOAT32_MAX;
    pointClamp.MaxAnisotropy = 1;
    CreateSampler(pointClamp);

    D3D12_SAMPLER_DESC pointWrap{};
    pointWrap.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    pointWrap.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    pointWrap.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    pointWrap.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    pointWrap.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    pointWrap.MaxLOD = D3D12_FLOAT32_MAX;
    pointWrap.MaxAnisotropy = 1;
    CreateSampler(pointWrap);

    D3D12_SAMPLER_DESC linearClamp{};
    linearClamp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    linearClamp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    linearClamp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    linearClamp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    linearClamp.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    linearClamp.MaxLOD = D3D12_FLOAT32_MAX;
    linearClamp.MaxAnisotropy = 1;
    CreateSampler(linearClamp);

    D3D12_SAMPLER_DESC linearWrap{};
    linearWrap.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    linearWrap.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    linearWrap.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    linearWrap.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    linearWrap.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    linearWrap.MaxLOD = D3D12_FLOAT32_MAX;
    linearWrap.MaxAnisotropy = 1;
    CreateSampler(linearWrap);

    D3D12_SAMPLER_DESC anisotropicClamp{};
    anisotropicClamp.Filter = D3D12_FILTER_ANISOTROPIC;
    anisotropicClamp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    anisotropicClamp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    anisotropicClamp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    anisotropicClamp.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    anisotropicClamp.MaxLOD = D3D12_FLOAT32_MAX;
    anisotropicClamp.MaxAnisotropy = 16;
    CreateSampler(anisotropicClamp);

    D3D12_SAMPLER_DESC anisotropicWrap{};
    anisotropicWrap.Filter = D3D12_FILTER_ANISOTROPIC;
    anisotropicWrap.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    anisotropicWrap.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    anisotropicWrap.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    anisotropicWrap.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    anisotropicWrap.MaxLOD = D3D12_FLOAT32_MAX;
    anisotropicWrap.MaxAnisotropy = 16;
    CreateSampler(anisotropicWrap);
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

bool SamplerManager::BindSampler(ShaderVariableBinder *shaderBinder, const std::string &nameKey, DefaultSampler defaultSampler) {
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
