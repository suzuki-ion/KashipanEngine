#pragma once

#include <cstdint>
#include <d3d12.h>

namespace KashipanEngine {

/// @brief シェーダーから "テクスチャとして" 参照できるSRV提供インターフェース
/// @details TextureManager のアセットテクスチャ / ScreenBuffer の描画結果などを共通化する。
class IShaderTexture {
public:
    virtual ~IShaderTexture() = default;

    virtual D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandle() const noexcept = 0;
    virtual std::uint32_t GetWidth() const noexcept = 0;
    virtual std::uint32_t GetHeight() const noexcept = 0;

protected:
    IShaderTexture() = default;

    IShaderTexture(const IShaderTexture&) = delete;
    IShaderTexture& operator=(const IShaderTexture&) = delete;
    IShaderTexture(IShaderTexture&&) = delete;
    IShaderTexture& operator=(IShaderTexture&&) = delete;
};

} // namespace KashipanEngine
