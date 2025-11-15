#pragma once
#include <string>
#include <string_view>
#include <utility>
#include <d3d12.h>
#include "Graphics/Resources/IGraphicsResource.h"
#include "ShaderCompiler.h"
#include "VectorMap.h"

namespace MyStd {

// ===== NameMap (既存) =====
template<class T>
class NameMap {
public:
    using key_type = std::string;
    using mapped_type = T;
    using entry_type = Entry<key_type, mapped_type>;
    entry_type &operator[](std::string_view name) { return storage_[key_type(name)]; }
    mapped_type &Set(std::string_view name, const mapped_type &v) { auto &e = storage_[key_type(name)]; e = v; return e.value; }
    template<class... Args> mapped_type &Emplace(std::string_view name, Args&&... args) { auto &e = storage_[key_type(name)]; e = mapped_type(std::forward<Args>(args)...); return e.value; }
    mapped_type *TryGet(std::string_view name) { auto it = storage_.find(key_type(name)); return it == storage_.end() ? nullptr : &it->value; }
    const mapped_type *TryGet(std::string_view name) const {
        auto &s = const_cast<Storage&>(storage_);
        auto it = s.find(key_type(name));
        return it == s.end() ? nullptr : &it->value;
    }
    mapped_type &Get(std::string_view name) { return storage_.at(key_type(name)).value; }
    const mapped_type &Get(std::string_view name) const { return const_cast<NameMap*>(this)->Get(name); }
    bool Contains(std::string_view name) const { return TryGet(name) != nullptr; }
    void Erase(std::string_view name) { storage_.erase(key_type(name)); }
    void Erase(size_t index) { storage_.erase(index); }
    size_t Size() const { return storage_.size(); }
    bool Empty() const { return storage_.empty(); }
    void Clear() { storage_.clear(); }
    entry_type &At(size_t index) { return storage_.at(index); }
    const entry_type &At(size_t index) const { return const_cast<NameMap*>(this)->At(index); }
    size_t IndexOf(std::string_view name) { return storage_.at(key_type(name)).index; }
    auto begin() { return storage_.begin(); }
    auto end() { return storage_.end(); }
    auto begin() const { return const_cast<NameMap*>(this)->begin(); }
    auto end() const { return const_cast<NameMap*>(this)->end(); }
private:
    using Storage = VectorMap<key_type, mapped_type>;
    Storage storage_;
};

} // namespace MyStd

namespace KashipanEngine {

struct ShaderVariableBinding {
    std::string name;
    D3D_SHADER_INPUT_TYPE type{};
    UINT bindPoint = 0;
    UINT bindCount = 0;
    UINT numSamples = 0;
    UINT space = 0;
    UINT flags = 0;
};

inline MyStd::NameMap<ShaderVariableBinding> CreateShaderVariableMap(const ShaderCompiler::ShaderCompiledInfo &compiled, bool appendSpace = true) {
    MyStd::NameMap<ShaderVariableBinding> map;
    const auto &refl = compiled.GetReflectionInfo();
    for (const auto &kv : refl.ResourceBindings()) {
        const auto &rb = kv.second;
        ShaderVariableBinding binding{};
        binding.name      = rb.Name();
        binding.type      = rb.Type();
        binding.bindPoint = rb.BindPoint();
        binding.bindCount = rb.BindCount();
        binding.numSamples= rb.NumSamples();
        binding.space     = rb.Space();
        binding.flags     = rb.Flags();
        std::string key = appendSpace ? (binding.name + "#s" + std::to_string(binding.space)) : binding.name;
        map.Set(key, binding);
    }
    return map;
}

// === Binder 追加 ===
// RootSignature 上の一つのリソースの最終的な位置情報
struct ShaderBindLocation {
    UINT rootParameterIndex = 0;  // 対応する Root Parameter
    UINT descriptorOffset    = 0; // Descriptor Table 内オフセット（Root Descriptorなら 0）
    bool isDescriptorTable   = true;
    bool isRootCBV           = false; // SetGraphicsRootConstantBufferView を使う
    bool isRootSRV           = false; // SetGraphicsRootShaderResourceView (必要なら)
    bool isRootUAV           = false; // SetGraphicsRootUnorderedAccessView (必要なら)
};

struct ShaderResourceKey {
    D3D_SHADER_INPUT_TYPE type{};
    UINT bindPoint = 0;
    UINT space = 0;
    bool operator==(const ShaderResourceKey &o) const {
        return type == o.type && bindPoint == o.bindPoint && space == o.space;
    }
};

// ハッシュ
struct ShaderResourceKeyHasher {
    size_t operator()(const ShaderResourceKey &k) const noexcept {
        return (static_cast<size_t>(k.type) << 24) ^ (static_cast<size_t>(k.space) << 12) ^ k.bindPoint;
    }
};

// Binder 本体
class ShaderVariableBinder {
public:
    // レイアウト登録: Descriptor Range 情報などから (type, register, space) → location を追加
    void RegisterDescriptorTableRange(
        D3D_SHADER_INPUT_TYPE type,
        UINT baseRegister,
        UINT space,
        UINT count,
        UINT rootParameterIndex,
        UINT startingOffsetInTable = 0
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

    // Root Descriptor (CBV/SRV/UAV) 登録
    void RegisterRootDescriptor(
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

    // 名前からバインド（NameMap を使用）
    bool BindByName(
        MyStd::NameMap<ShaderVariableBinding> &nameMap,
        ID3D12GraphicsCommandList *cmd,
        std::string_view nameKey,
        IGraphicsResource *resource,
        D3D12_GPU_DESCRIPTOR_HANDLE srvUavCbvHeapStart,
        UINT descriptorIncrementSize
    ) {
        auto *binding = nameMap.TryGet(nameKey);
        if (!binding) return false;
        ShaderResourceKey key{ binding->type, binding->bindPoint, binding->space };
        auto it = locations_.find(key);
        if (it == locations_.end()) return false;

        const ShaderBindLocation &loc = it->second;

        if (loc.isDescriptorTable) {
            // GPU ハンドルオフセット計算
            D3D12_GPU_DESCRIPTOR_HANDLE handle{};
            handle.ptr = srvUavCbvHeapStart.ptr + static_cast<UINT64>(loc.descriptorOffset) * descriptorIncrementSize;
            cmd->SetGraphicsRootDescriptorTable(loc.rootParameterIndex, handle);
            return true;
        } else {
            // Root Descriptor
            if (loc.isRootCBV) {
                if (!resource || !resource->GetResource()) return false;
                cmd->SetGraphicsRootConstantBufferView(loc.rootParameterIndex, resource->GetResource()->GetGPUVirtualAddress());
                return true;
            }
            if (loc.isRootSRV) {
                if (!resource || !resource->GetResource()) return false;
                cmd->SetGraphicsRootShaderResourceView(loc.rootParameterIndex, resource->GetResource()->GetGPUVirtualAddress());
                return true;
            }
            if (loc.isRootUAV) {
                if (!resource || !resource->GetResource()) return false;
                cmd->SetGraphicsRootUnorderedAccessView(loc.rootParameterIndex, resource->GetResource()->GetGPUVirtualAddress());
                return true;
            }
        }
        return false;
    }

private:
    std::unordered_map<ShaderResourceKey, ShaderBindLocation, ShaderResourceKeyHasher> locations_;
};

} // namespace KashipanEngine
