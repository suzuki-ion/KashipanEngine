#pragma once
#include <string>
#include <string_view>
#include <utility>
#include <d3d12.h>
#include "Graphics/Resources.h"
#include "ShaderCompiler.h"
#include "NameMap.h"
#include <unordered_map>
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

struct PipelineInfo;
class PipelineCreator;

/// @brief シェーダーステージ種類
enum class ShaderStage : UINT {
    Unknown = 0,
    Vertex,
    Pixel,
    Geometry,
    Hull,
    Domain
};

/// @brief シェーダー変数バインディング情報構造体
struct ShaderVariableBinding {
    const std::string &Name() const { return name; }
    D3D_SHADER_INPUT_TYPE Type() const { return type; }
    UINT BindPoint() const { return bindPoint; }
    UINT BindCount() const { return bindCount; }
    UINT NumSamples() const { return numSamples; }
    UINT Space() const { return space; }
    UINT Flags() const { return flags; }
private:
    friend inline MyStd::NameMap<ShaderVariableBinding> CreateShaderVariableMap(const ShaderCompiler::ShaderCompiledInfo &compiled, bool appendSpace);
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

struct ShaderBindLocation {
    UINT rootParameterIndex  = 0;
    UINT descriptorOffset    = 0;
    bool isDescriptorTable   = true;
    bool isRootCBV           = false;
    bool isRootSRV           = false;
    bool isRootUAV           = false;
    ShaderStage stage        = ShaderStage::Unknown;
};

struct ShaderResourceKey {
    D3D_SHADER_INPUT_TYPE type{};
    UINT bindPoint = 0;
    UINT space = 0;
    ShaderStage stage = ShaderStage::Unknown;
    bool operator==(const ShaderResourceKey &o) const {
        return type == o.type && bindPoint == o.bindPoint && space == o.space && stage == o.stage;
    }
};

struct ShaderResourceKeyHasher {
    size_t operator()(const ShaderResourceKey &k) const noexcept {
        return (static_cast<size_t>(k.type) << 28) ^ (static_cast<size_t>(k.space) << 20) ^ (static_cast<size_t>(k.stage) << 12) ^ k.bindPoint;
    }
};

/// @brief シェーダー変数バインダークラス
class ShaderVariableBinder {
public:
    ShaderVariableBinder(Passkey<PipelineInfo>);
    void SetCommandList(ID3D12GraphicsCommandList* cmd);
    void SetNameMap(const MyStd::NameMap<ShaderVariableBinding>& nameMap);
    const MyStd::NameMap<ShaderVariableBinding>& GetNameMap() const;

    /// @brief デスクリプタテーブル範囲を登録（PipelineCreator専用）
    /// @param type デスクリプタタイプ
    /// @param baseRegister ベースレジスター
    /// @param space レジスタースペース
    /// @param count 範囲の数
    /// @param rootParameterIndex ルートパラメーターインデックス
    /// @param startingOffsetInTable テーブル内の開始オフセット
    /// @param stage 対象シェーダーステージ
    void RegisterDescriptorTableRange(
        Passkey<PipelineCreator>,
        D3D_SHADER_INPUT_TYPE type,
        UINT baseRegister,
        UINT space,
        UINT count,
        UINT rootParameterIndex,
        UINT startingOffsetInTable = 0,
        ShaderStage stage = ShaderStage::Unknown
    );

    /// @brief ルートデスクリプタを登録（PipelineCreator専用）
    /// @param type デスクリプタタイプ
    /// @param registerIndex レジスターインデックス
    /// @param space レジスタースペース
    /// @param rootParameterIndex ルートパラメーターインデックス
    /// @param stage 対象シェーダーステージ
    void RegisterRootDescriptor(
        Passkey<PipelineCreator>,
        D3D_SHADER_INPUT_TYPE type,
        UINT registerIndex,
        UINT space,
        UINT rootParameterIndex,
        ShaderStage stage = ShaderStage::Unknown
    );

    /// @brief SRV/UAV/CBVをバインド
    /// @param nameKey バインド名キー（例:"Vertex:MyTexture#s0"）
    /// @param resource バインドするリソース
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す
    bool Bind(const std::string& nameKey, IGraphicsResource* resource);

    /// @brief デスクリプタテーブル内のSRV/UAV/CBVをバインド
    /// @param nameKey バインド名キー
    /// @param descriptorHandle デスクリプタハンドル
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す
    bool Bind(const std::string& nameKey, D3D12_GPU_DESCRIPTOR_HANDLE descriptorHandle);

private:
    ID3D12GraphicsCommandList* cmd_ = nullptr;
    MyStd::NameMap<ShaderVariableBinding> nameMap_;
    std::unordered_map<ShaderResourceKey, ShaderBindLocation, ShaderResourceKeyHasher> locations_;

    static ShaderStage StageFromNameKey(const std::string& nameKey);
};

} // namespace KashipanEngine
