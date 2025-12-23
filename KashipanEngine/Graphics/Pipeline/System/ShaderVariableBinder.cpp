#include "ShaderVariableBinder.h"

namespace KashipanEngine {

ShaderVariableBinder::ShaderVariableBinder(Passkey<PipelineInfo>) {}

void ShaderVariableBinder::SetCommandList(ID3D12GraphicsCommandList* cmd) { cmd_ = cmd; }

void ShaderVariableBinder::SetNameMap(const MyStd::NameMap<ShaderVariableBinding>& nameMap) { nameMap_ = nameMap; }

const MyStd::NameMap<ShaderVariableBinding>& ShaderVariableBinder::GetNameMap() const { return nameMap_; }

static ShaderStage VisibilityToStage(D3D12_SHADER_VISIBILITY vis) {
    switch (vis) {
    case D3D12_SHADER_VISIBILITY_VERTEX: return ShaderStage::Vertex;
    case D3D12_SHADER_VISIBILITY_PIXEL: return ShaderStage::Pixel;
    case D3D12_SHADER_VISIBILITY_GEOMETRY: return ShaderStage::Geometry;
    case D3D12_SHADER_VISIBILITY_HULL: return ShaderStage::Hull;
    case D3D12_SHADER_VISIBILITY_DOMAIN: return ShaderStage::Domain;
    default: return ShaderStage::Unknown;
    }
}

ShaderStage ShaderVariableBinder::StageFromNameKey(const std::string& nameKey) {
    // expects prefix like "Vertex:" or "Pixel:"; Unknown if not present
    if (nameKey.rfind("Vertex:", 0) == 0) return ShaderStage::Vertex;
    if (nameKey.rfind("Pixel:", 0) == 0) return ShaderStage::Pixel;
    if (nameKey.rfind("Geometry:", 0) == 0) return ShaderStage::Geometry;
    if (nameKey.rfind("Hull:", 0) == 0) return ShaderStage::Hull;
    if (nameKey.rfind("Domain:", 0) == 0) return ShaderStage::Domain;
    return ShaderStage::Unknown;
}

void ShaderVariableBinder::RegisterDescriptorTableRange(
    Passkey<PipelineCreator>,
    D3D_SHADER_INPUT_TYPE type,
    UINT baseRegister,
    UINT space,
    UINT count,
    UINT rootParameterIndex,
    UINT startingOffsetInTable,
    ShaderStage stage
) {
    for (UINT i = 0; i < count; ++i) {
        ShaderResourceKey key{ type, baseRegister + i, space, stage };
        ShaderBindLocation loc{};
        loc.rootParameterIndex = rootParameterIndex;
        loc.descriptorOffset   = startingOffsetInTable + i;
        loc.isDescriptorTable  = true;
        loc.stage = stage;
        locations_[key] = loc;
    }
}

void ShaderVariableBinder::RegisterRootDescriptor(
    Passkey<PipelineCreator>,
    D3D_SHADER_INPUT_TYPE type,
    UINT registerIndex,
    UINT space,
    UINT rootParameterIndex,
    ShaderStage stage
) {
    ShaderResourceKey key{ type, registerIndex, space, stage };
    ShaderBindLocation loc{};
    loc.rootParameterIndex = rootParameterIndex;
    loc.isDescriptorTable  = false;
    loc.stage = stage;
    if (type == D3D_SIT_CBUFFER) loc.isRootCBV = true;
    else if (type == D3D_SIT_STRUCTURED || type == D3D_SIT_BYTEADDRESS || type == D3D_SIT_TBUFFER || type == D3D_SIT_TEXTURE) loc.isRootSRV = true;
    else if (type == D3D_SIT_UAV_RWTYPED || type == D3D_SIT_UAV_RWSTRUCTURED || type == D3D_SIT_UAV_RWBYTEADDRESS) loc.isRootUAV = true;
    locations_[key] = loc;
}

bool ShaderVariableBinder::Bind(const std::string& nameKey, IGraphicsResource* resource) {
    auto* binding = nameMap_.TryGet(nameKey);
    if (!binding || !cmd_ || !resource || !resource->GetResource()) return false;
    ShaderStage stage = StageFromNameKey(nameKey);
    ShaderResourceKey key{ binding->Type(), binding->BindPoint(), binding->Space(), stage };
    auto it = locations_.find(key);
    if (it == locations_.end()) return false;
    const ShaderBindLocation& loc = it->second;
    resource->SetCommandList(cmd_);
    if (loc.isRootCBV) {
        cmd_->SetGraphicsRootConstantBufferView(loc.rootParameterIndex, resource->GetResource()->GetGPUVirtualAddress());
        return true;
    }
    if (loc.isRootSRV) {
        cmd_->SetGraphicsRootShaderResourceView(loc.rootParameterIndex, resource->GetResource()->GetGPUVirtualAddress());
        return true;
    }
    if (loc.isRootUAV) {
        cmd_->SetGraphicsRootUnorderedAccessView(loc.rootParameterIndex, resource->GetResource()->GetGPUVirtualAddress());
        return true;
    }
    return false;
}

bool ShaderVariableBinder::Bind(const std::string& nameKey, D3D12_GPU_DESCRIPTOR_HANDLE descriptorHandle) {
    auto* binding = nameMap_.TryGet(nameKey);
    if (!binding || !cmd_) return false;
    ShaderStage stage = StageFromNameKey(nameKey);
    ShaderResourceKey key{ binding->Type(), binding->BindPoint(), binding->Space(), stage };
    auto it = locations_.find(key);
    if (it == locations_.end()) return false;
    const ShaderBindLocation& loc = it->second;
    if (!loc.isDescriptorTable) return false;

    if (loc.descriptorOffset != 0) {
        auto *device = IGraphicsResource::GetDevice({});
        if (!device) return false;

        const bool isSampler = (binding->Type() == D3D_SIT_SAMPLER);
        const UINT inc = device->GetDescriptorHandleIncrementSize(
            isSampler ? D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER : D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        descriptorHandle.ptr += static_cast<UINT64>(loc.descriptorOffset) * static_cast<UINT64>(inc);
    }

    cmd_->SetGraphicsRootDescriptorTable(loc.rootParameterIndex, descriptorHandle);
    return true;
}

} // namespace KashipanEngine
