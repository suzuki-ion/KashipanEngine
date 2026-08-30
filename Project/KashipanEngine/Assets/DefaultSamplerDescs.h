#pragma once
#include <array>
#include <d3d12.h>

namespace KashipanEngine {

/// @brief 既定サンプラーの種類数（DefaultSampler enum、gSamplers[]静的サンプラー配列と一致させること）
inline constexpr UINT kDefaultSamplerCount = 6;

/// @brief 既定サンプラー1件分のパラメータ（Filter/AddressMode/異方性フィルタリング段数のみ可変）
struct DefaultSamplerDesc {
    D3D12_FILTER filter;
    D3D12_TEXTURE_ADDRESS_MODE addressMode; // U/V/W共通
    UINT maxAnisotropy;
};

/// @brief 既定6種のサンプラー設定（SamplerManagerの動的サンプラー生成・PipelineCreatorの静的サンプラー
///        （gSamplers[6]）生成の両方から参照される唯一の定義元。並び順はDefaultSampler enumと一致させること
///        （PointClamp, PointWrap, LinearClamp, LinearWrap, AnisotropicClamp, AnisotropicWrap）
inline const std::array<DefaultSamplerDesc, kDefaultSamplerCount> &GetDefaultSamplerDescs() {
    static const std::array<DefaultSamplerDesc, kDefaultSamplerCount> descs = { {
        { D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 1 },  // PointClamp
        { D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 1 },   // PointWrap
        { D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 1 }, // LinearClamp
        { D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 1 },  // LinearWrap
        { D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 16 },       // AnisotropicClamp
        { D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 16 },        // AnisotropicWrap
    } };
    return descs;
}

} // namespace KashipanEngine
