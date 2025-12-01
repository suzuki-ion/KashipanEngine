#include "ShaderVariableBinder.h"

namespace KashipanEngine {

ShaderVariableBinder::ShaderVariableBinder(Passkey<PipelineInfo>) {}

void ShaderVariableBinder::SetCommandList(ID3D12GraphicsCommandList* cmd) { cmd_ = cmd; }

void ShaderVariableBinder::SetNameMap(const MyStd::NameMap<ShaderVariableBinding>& nameMap) { nameMap_ = nameMap; }

const MyStd::NameMap<ShaderVariableBinding>& ShaderVariableBinder::GetNameMap() const { return nameMap_; }

void ShaderVariableBinder::RegisterDescriptorTableRange(
    Passkey<PipelineCreator>,
    D3D_SHADER_INPUT_TYPE type,
    UINT baseRegister,
    UINT space,
    UINT count,
    UINT rootParameterIndex,
    UINT startingOffsetInTable
) {
    for (UINT i = 0; i < count; ++i) {
        ShaderResourceKey key{ type, baseRegister + i, space };
        ShaderBindLocation loc{};
        loc.rootParameterIndex = rootParameterIndex;
        loc.descriptorOffset   = startingOffsetInTable + i;
        loc.isDescriptorTable  = true;
        locations_[key] = loc;
    }
}

void ShaderVariableBinder::RegisterRootDescriptor(
    Passkey<PipelineCreator>,
    D3D_SHADER_INPUT_TYPE type,
    UINT registerIndex,
    UINT space,
    UINT rootParameterIndex
) {
    ShaderResourceKey key{ type, registerIndex, space };
    ShaderBindLocation loc{};
    loc.rootParameterIndex = rootParameterIndex;
    loc.isDescriptorTable  = false;
    if (type == D3D_SIT_CBUFFER) loc.isRootCBV = true;
    else if (type == D3D_SIT_STRUCTURED || type == D3D_SIT_BYTEADDRESS || type == D3D_SIT_TBUFFER || type == D3D_SIT_TEXTURE) loc.isRootSRV = true;
    else if (type == D3D_SIT_UAV_RWTYPED || type == D3D_SIT_UAV_RWSTRUCTURED || type == D3D_SIT_UAV_RWBYTEADDRESS) loc.isRootUAV = true;
    locations_[key] = loc;
}

bool ShaderVariableBinder::Bind(const std::string& nameKey, IGraphicsResource* resource) {
    auto* binding = nameMap_.TryGet(nameKey);
    if (!binding || !cmd_ || !resource || !resource->GetResource()) return false;
    ShaderResourceKey key{ binding->Type(), binding->BindPoint(), binding->Space() };
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
    ShaderResourceKey key{ binding->Type(), binding->BindPoint(), binding->Space() };
    auto it = locations_.find(key);
    if (it == locations_.end()) return false;
    const ShaderBindLocation& loc = it->second;
    if (!loc.isDescriptorTable) return false;
    cmd_->SetGraphicsRootDescriptorTable(loc.rootParameterIndex, descriptorHandle);
    return true;
}

} // namespace KashipanEngine
